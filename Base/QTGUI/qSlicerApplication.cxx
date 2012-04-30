/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// Qt includes
#include <QAction>
#include <QDebug>
#include <QMainWindow>

#include "vtkSlicerConfigure.h" // For Slicer_USE_QtTesting

// CTK includes
#include <ctkColorDialog.h>
#include <ctkErrorLogModel.h>
#include <ctkMessageBox.h>
#include <ctkSettings.h>
#ifdef Slicer_USE_QtTesting
#include <ctkQtTestingUtility.h>
#include <ctkXMLEventObserver.h>
#include <ctkXMLEventSource.h>
#endif
#include <ctkToolTipTrapper.h>

// QTGUI includes
#include "qSlicerAbstractModule.h"
#include "qSlicerApplication.h"
#include "qSlicerCommandOptions.h"
#include "qSlicerCoreApplication_p.h"
#include "qSlicerIOManager.h"
#include "qSlicerLayoutManager.h"
#include "qSlicerModuleManager.h"
#ifdef Slicer_USE_PYTHONQT
# include "qSlicerPythonManager.h"
#endif
#ifdef Slicer_BUILD_EXTENSIONMANAGER_SUPPORT
# include "qSlicerSettingsExtensionsPanel.h"
#endif
#include "qSlicerSettingsCachePanel.h"
#include "qSlicerSettingsGeneralPanel.h"
#include "qSlicerSettingsModulesPanel.h"

// qMRMLWidget includes
#include "qMRMLEventBrokerConnection.h"

// qMRML includes
#ifdef Slicer_USE_QtTesting
#include <qMRMLCheckableNodeComboBoxEventPlayer.h>
#include <qMRMLNodeComboBoxEventPlayer.h>
#include <qMRMLNodeComboBoxEventTranslator.h>
#include <qMRMLTreeViewEventPlayer.h>
#include <qMRMLTreeViewEventTranslator.h>
#endif

// Logic includes
#include <vtkSlicerApplicationLogic.h>

// MRML includes
#include <vtkMRMLNode.h>

// VTK includes
#include <vtkNew.h>

//-----------------------------------------------------------------------------
class qSlicerApplicationPrivate : public qSlicerCoreApplicationPrivate
{
  Q_DECLARE_PUBLIC(qSlicerApplication);
protected:
  qSlicerApplication* const q_ptr;
public:
  typedef qSlicerCoreApplicationPrivate Superclass;

  qSlicerApplicationPrivate(qSlicerApplication& object, qSlicerCommandOptions * commandOptions, qSlicerIOManager * ioManager);
  virtual ~qSlicerApplicationPrivate();

  /// Convenient method regrouping all initialization code
  virtual void init();

  /// Initialize application style
  void initStyle();

  qSlicerLayoutManager*   LayoutManager;
  ctkToolTipTrapper*      ToolTipTrapper;
  ctkSettingsDialog*      SettingsDialog;
#ifdef Slicer_USE_QtTesting
  ctkQtTestingUtility*    TestingUtility;
#endif
};


//-----------------------------------------------------------------------------
// qSlicerApplicationPrivate methods

//-----------------------------------------------------------------------------
qSlicerApplicationPrivate::qSlicerApplicationPrivate(
  qSlicerApplication& object, qSlicerCommandOptions * commandOptions, qSlicerIOManager * ioManager)
  : qSlicerCoreApplicationPrivate(object, commandOptions, ioManager), q_ptr(&object)
{
  this->LayoutManager = 0;
  this->ToolTipTrapper = 0;
  this->SettingsDialog = 0;
#ifdef Slicer_USE_QtTesting
  this->TestingUtility = 0;
#endif
}

//-----------------------------------------------------------------------------
qSlicerApplicationPrivate::~qSlicerApplicationPrivate()
{
  delete this->SettingsDialog;
  this->SettingsDialog = 0;
#ifdef Slicer_USE_QtTesing
  delete this->TestingUtility;
  this->TestingUtility = 0;
#endif
}

//-----------------------------------------------------------------------------
void qSlicerApplicationPrivate::init()
{
  Q_Q(qSlicerApplication);

  ctkVTKConnectionFactory::setInstance(new qMRMLConnectionFactory);

#ifdef Slicer_USE_PYTHONQT
  if (!qSlicerCoreApplication::testAttribute(qSlicerCoreApplication::AA_DisablePython))
    {
    // Note: qSlicerCoreApplication class takes ownership of the pythonManager and
    // will be responsible to delete it
    q->setCorePythonManager(new qSlicerPythonManager());
    }
#endif

  this->Superclass::init();

  this->initStyle();

  this->ToolTipTrapper = new ctkToolTipTrapper(q);
  this->ToolTipTrapper->setToolTipsTrapped(false);
  this->ToolTipTrapper->setToolTipsWordWrapped(true);

  //----------------------------------------------------------------------------
  // Settings Dialog
  //----------------------------------------------------------------------------
  this->SettingsDialog = new ctkSettingsDialog(0);

  this->SettingsDialog->addPanel("General settings", new qSlicerSettingsGeneralPanel);

  qSlicerSettingsModulesPanel * settingsModulesPanel = new qSlicerSettingsModulesPanel;
  this->SettingsDialog->addPanel("Modules settings", settingsModulesPanel);
  settingsModulesPanel->setRestartRequested(false);

#ifdef Slicer_BUILD_EXTENSIONMANAGER_SUPPORT
  qSlicerSettingsExtensionsPanel * settingsExtensionsPanel = new qSlicerSettingsExtensionsPanel;
  this->SettingsDialog->addPanel("Extensions settings", settingsExtensionsPanel);
  settingsExtensionsPanel->setRestartRequested(false);
#endif
  qSlicerSettingsCachePanel* cachePanel = new qSlicerSettingsCachePanel;
  cachePanel->setCacheManager(this->MRMLScene->GetCacheManager());
  this->SettingsDialog->addPanel("Cache settings", cachePanel);

  QObject::connect(this->SettingsDialog, SIGNAL(accepted()),
                   q, SLOT(onSettingDialogAccepted()));

  //----------------------------------------------------------------------------
  // Test Utility
  //----------------------------------------------------------------------------
#ifdef Slicer_USE_QtTesting
  this->TestingUtility = new ctkQtTestingUtility(0);
  this->TestingUtility->addEventObserver(
      "xml", new ctkXMLEventObserver(this->TestingUtility));
  ctkXMLEventSource* eventSource = new ctkXMLEventSource(this->TestingUtility);
  eventSource->setRestoreSettingsAuto(
      qSlicerApplication::testAttribute(qSlicerCoreApplication::AA_EnableTesting));
  this->TestingUtility->addEventSource("xml", eventSource);

  // Translator and Player for MRML widget
  this->TestingUtility->addPlayer(
      new qMRMLCheckableNodeComboBoxEventPlayer());
  this->TestingUtility->addPlayer(
      new qMRMLNodeComboBoxEventPlayer());
  this->TestingUtility->addTranslator(
      new qMRMLNodeComboBoxEventTranslator());
  this->TestingUtility->addPlayer(
      new qMRMLTreeViewEventPlayer());
  this->TestingUtility->addTranslator(
      new qMRMLTreeViewEventTranslator());

  // Player for the CLI Module || cannot be added for the moment ...
#endif
}
/*
#if !defined (QT_NO_LIBRARY) && !defined(QT_NO_SETTINGS)
Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, loaderV2,
    (QIconEngineFactoryInterfaceV2_iid, QLatin1String("/iconengines"), Qt::CaseInsensitive))
#endif
*/
//-----------------------------------------------------------------------------
void qSlicerApplicationPrivate::initStyle()
{
  // Force showing the icons in the menus even if the native OS style
  // discourages it
  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
}

//-----------------------------------------------------------------------------
// qSlicerApplication methods

//-----------------------------------------------------------------------------
qSlicerApplication::qSlicerApplication(int &_argc, char **_argv)
  : Superclass(new qSlicerApplicationPrivate(*this, new qSlicerCommandOptions, 0), _argc, _argv)
{
  Q_D(qSlicerApplication);
  d->init();
  // Note: Since QWidget/QDialog requires a QApplication to be successfully instantiated,
  //       qSlicerIOManager is not added to the constructor initialization list.
  //       Indeed, internally qSlicerIOManager registers qSlicerDataDialog, ...
  d->CoreIOManager = QSharedPointer<qSlicerIOManager>(new qSlicerIOManager);
}

//-----------------------------------------------------------------------------
qSlicerApplication::~qSlicerApplication()
{
}

//-----------------------------------------------------------------------------
qSlicerApplication* qSlicerApplication::application()
{
  qSlicerApplication* app = qobject_cast<qSlicerApplication*>(QApplication::instance());
  return app;
}

//-----------------------------------------------------------------------------
qSlicerCommandOptions* qSlicerApplication::commandOptions()
{
  qSlicerCommandOptions* _commandOptions =
    dynamic_cast<qSlicerCommandOptions*>(this->coreCommandOptions());
  Q_ASSERT(_commandOptions);
  return _commandOptions;
}

//-----------------------------------------------------------------------------
qSlicerIOManager* qSlicerApplication::ioManager()
{
  qSlicerIOManager* _ioManager = dynamic_cast<qSlicerIOManager*>(this->coreIOManager());
  Q_ASSERT(_ioManager);
  return _ioManager;
}

#ifdef Slicer_USE_PYTHONQT
//-----------------------------------------------------------------------------
qSlicerPythonManager* qSlicerApplication::pythonManager()
{
  qSlicerPythonManager* _pythonManager = 0;
  if (!qSlicerCoreApplication::testAttribute(qSlicerCoreApplication::AA_DisablePython))
    {
    _pythonManager = qobject_cast<qSlicerPythonManager*>(this->corePythonManager());
    Q_ASSERT(_pythonManager);
    }

  return _pythonManager;
}
#endif

#ifdef Slicer_USE_QtTesting
//-----------------------------------------------------------------------------
ctkQtTestingUtility* qSlicerApplication::testingUtility()
{
  Q_D(const qSlicerApplication);
  return d->TestingUtility;
}
#endif

//-----------------------------------------------------------------------------
void qSlicerApplication::setLayoutManager(qSlicerLayoutManager* layoutManager)
{
  Q_D(qSlicerApplication);
  d->LayoutManager = layoutManager;
  if (this->applicationLogic())
    {
    this->applicationLogic()->SetSliceLogics(
      d->LayoutManager? d->LayoutManager->mrmlSliceLogics() : 0);
    if (d->LayoutManager)
      {
      d->LayoutManager->setMRMLColorLogic(this->applicationLogic()->GetColorLogic());
      }
    }
}

//-----------------------------------------------------------------------------
qSlicerLayoutManager* qSlicerApplication::layoutManager()const
{
  Q_D(const qSlicerApplication);
  return d->LayoutManager;
}

//-----------------------------------------------------------------------------
QMainWindow* qSlicerApplication::mainWindow()const
{
  foreach(QWidget * widget, this->topLevelWidgets())
    {
    QMainWindow* window = qobject_cast<QMainWindow*>(widget);
    if (window)
      {
      return window;
      }
    }
  return 0;
}

//-----------------------------------------------------------------------------
void qSlicerApplication::handleCommandLineArguments()
{
  qSlicerCommandOptions* options = this->commandOptions();
  Q_ASSERT(options);

  if (options->noMainWindow() || options->disableMessageHandlers())
    {
    // If no UI is expected, it doesn't make sens to use registered handlers.
    // Let's disable them.
    this->errorLogModel()->disableAllMsgHandler();
    }

  this->Superclass::handleCommandLineArguments();

  this->setToolTipsEnabled(!options->disableToolTips());
}

//-----------------------------------------------------------------------------
void qSlicerApplication::onSlicerApplicationLogicModified()
{
  if (this->layoutManager())
    {
    this->layoutManager()->setMRMLColorLogic(
      this->applicationLogic()->GetColorLogic());
    }
}

//-----------------------------------------------------------------------------
QSettings* qSlicerApplication::newSettings(const QString& fileName)
{
  if (!fileName.isEmpty())
    {
    // Special case for tmp settings
    return new ctkSettings(fileName, QSettings::defaultFormat(), this);
    }
  return new ctkSettings(this);
}

//-----------------------------------------------------------------------------
void qSlicerApplication::setToolTipsEnabled(bool enable)
{
  Q_D(qSlicerApplication);
  d->ToolTipTrapper->setToolTipsTrapped(!enable);
}

//-----------------------------------------------------------------------------
void qSlicerApplication::confirmRestart(QString reason)
{
  if (reason.isEmpty())
    {
    reason = tr("Are you sure you want to restart?");
    }

  ctkMessageBox confirmDialog;
  confirmDialog.setText(reason);
  confirmDialog.setIcon(QMessageBox::Question);
  confirmDialog.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
  confirmDialog.setDontShowAgainSettingsKey( "MainWindow/DontConfirmRestart" );
  bool restartConfirmed = (confirmDialog.exec() == QMessageBox::Ok);

  if (restartConfirmed)
    {
    this->restart();
    }
}

//-----------------------------------------------------------------------------
QString qSlicerApplication::nodeModule(vtkMRMLNode* node)const
{
  QString nodeClassName = node->GetClassName();
  if (node->IsA("vtkMRMLCameraNode") ||
      node->IsA("vtkMRMLViewNode"))
    {
    return "Cameras";
    }
  else if (node->IsA("vtkMRMLSliceNode") ||
           node->IsA("vtkMRMLSliceCompositeNode") ||
           node->IsA("vtkMRMLSliceLayerNode"))
    {
    return "SliceController";
    }
  else if (node->IsA("vtkMRMLAnnotationNode") ||
           node->IsA("vtkMRMLAnnotationDisplayNode") ||
           node->IsA("vtkMRMLAnnotationStorageNode") ||
           node->IsA("vtkMRMLAnnotationHierarchyNode"))
    {
    return "Annotations";
    }
  else if (node->IsA("vtkMRMLTransformNode") ||
           node->IsA("vtkMRMLTransformStorageNode"))
    {
    return "Transforms";
    }
  else if (node->IsA("vtkMRMLColorNode"))
    {
    return "Colors";
    }
  else if (nodeClassName.contains("vtkMRMLFiberBundle"))
    {
    return "TractographyDisplay";
    }
  else if (node->IsA("vtkMRMLModelNode") ||
           node->IsA("vtkMRMLModelDisplayNode") ||
           node->IsA("vtkMRMLModelHierarchyNode") ||
           node->IsA("vtkMRMLModelStorageNode"))
    {
    return "Models";
    }
  else if (node->IsA("vtkMRMLSceneViewNode") ||
           node->IsA("vtkMRMLSceneViewStorageNode"))
    {
    return "SceneViews";
    }
  else if (node->IsA("vtkMRMLVolumeNode") ||
           node->IsA("vtkMRMLVolumeDisplayNode") ||
           node->IsA("vtkMRMLVolumeArchetypeStorageNode") ||
           node->IsA("vtkMRMLVolumeHeaderlessStorageNode"))
    {
    return "Volumes";
    }
  else if (node->IsA("vtkMRMLVolumePropertyNode") ||
           node->IsA("vtkMRMLVolumePropertyStorageNode") ||
           node->IsA("vtkMRMLVolumeRenderingDisplayNode"))
    {
    return "VolumeRendering";
    }
  qWarning() << "Couldn't find a module for node class" << node->GetClassName();
  return "data";
}

//-----------------------------------------------------------------------------
void qSlicerApplication::openNodeModule(vtkMRMLNode* node)
{
  QString moduleName = this->nodeModule(node);
  qSlicerAbstractCoreModule* module = this->moduleManager()->module(moduleName);
  qSlicerAbstractModule* moduleWithAction = qobject_cast<qSlicerAbstractModule*>(module);
  if (moduleWithAction)
    {
    moduleWithAction->action()->trigger();
    }
}

// --------------------------------------------------------------------------
ctkSettingsDialog* qSlicerApplication::settingsDialog()const
{
  Q_D(const qSlicerApplication);
  return d->SettingsDialog;
}

// --------------------------------------------------------------------------
void qSlicerApplication::onSettingDialogAccepted()
{
  Q_D(qSlicerApplication);
  QStringList reasons;

  qSlicerSettingsModulesPanel *settingsModulesPanel =
    qobject_cast<qSlicerSettingsModulesPanel*>(
      d->SettingsDialog->panel("Modules settings"));
  Q_ASSERT(settingsModulesPanel);
  if (settingsModulesPanel->restartRequested())
    {
    reasons << "Module paths have been updated.";
    }

#ifdef Slicer_BUILD_EXTENSIONMANAGER_SUPPORT
  qSlicerSettingsExtensionsPanel *settingsExtensionsPanel =
    qobject_cast<qSlicerSettingsExtensionsPanel*>(
      d->SettingsDialog->panel("Extensions settings"));
  Q_ASSERT(settingsExtensionsPanel);
  if (settingsExtensionsPanel->restartRequested())
    {
    reasons << "Extension manager visibility has been updated.";
    }
#endif

  if (reasons.count() > 0)
    {
    QString formattedReasons;
    foreach(const QString& reason, reasons)
      {
      formattedReasons += QString("<li>%1</li>").arg(reason);
      }
    this->confirmRestart(QString("Do you want to restart now?"
                                 "<ul>%1</ul>").arg(formattedReasons));
    }
}
