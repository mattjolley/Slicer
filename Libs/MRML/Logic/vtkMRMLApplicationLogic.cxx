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

// vtkSlicer includes
#include "vtkArchive.h" // note: this is not a class

// MRMLLogic includes
#include "vtkMRMLApplicationLogic.h"
#include "vtkMRMLColorLogic.h"
#include "vtkMRMLSliceLogic.h"
#include <vtkMRMLSliceLinkLogic.h>
#include <vtkMRMLModelHierarchyLogic.h>

// MRML includes
#include <vtkMRMLInteractionNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLStorableNode.h>
#include <vtkMRMLStorageNode.h>
#include <vtkMRMLSceneViewNode.h>

// VTK includes
#include <vtkCollection.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

// VTKSYS includes
#include <vtksys/SystemTools.hxx>
#include <vtksys/Glob.hxx>

// STD includes
#include <cassert>
#include <sstream>

// For LoadDefaultParameterSets
#ifdef WIN32
# include <windows.h>
#else
# include <dirent.h>
# include <errno.h>
#endif

//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkMRMLApplicationLogic, "$Revision$");
vtkStandardNewMacro(vtkMRMLApplicationLogic);

//----------------------------------------------------------------------------
class vtkMRMLApplicationLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkMRMLSelectionNode>    SelectionNode;
  vtkSmartPointer<vtkMRMLInteractionNode>  InteractionNode;
  vtkSmartPointer<vtkCollection> SliceLogics;
  vtkSmartPointer<vtkMRMLSliceLinkLogic> SliceLinkLogic;
  vtkSmartPointer<vtkMRMLModelHierarchyLogic> ModelHierarchyLogic;
  vtkSmartPointer<vtkMRMLColorLogic> ColorLogic;
};

//----------------------------------------------------------------------------
// vtkInternal methods

//----------------------------------------------------------------------------
vtkMRMLApplicationLogic::vtkInternal::vtkInternal()
{
  this->SliceLinkLogic = vtkSmartPointer<vtkMRMLSliceLinkLogic>::New();
  this->ModelHierarchyLogic = vtkSmartPointer<vtkMRMLModelHierarchyLogic>::New();
  this->ColorLogic = vtkSmartPointer<vtkMRMLColorLogic>::New();
}

//----------------------------------------------------------------------------
vtkMRMLApplicationLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
// vtkMRMLApplicationLogic methods

//----------------------------------------------------------------------------
vtkMRMLApplicationLogic::vtkMRMLApplicationLogic()
{
  this->Internal = new vtkInternal;
  this->Internal->SliceLinkLogic->SetMRMLApplicationLogic(this);
  this->Internal->ModelHierarchyLogic->SetMRMLApplicationLogic(this);
  this->Internal->ColorLogic->SetMRMLApplicationLogic(this);
}

//----------------------------------------------------------------------------
vtkMRMLApplicationLogic::~vtkMRMLApplicationLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkMRMLApplicationLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
vtkMRMLSelectionNode * vtkMRMLApplicationLogic::GetSelectionNode()const
{
  return this->Internal->SelectionNode;
}

//----------------------------------------------------------------------------
vtkMRMLInteractionNode * vtkMRMLApplicationLogic::GetInteractionNode()const
{
  return this->Internal->InteractionNode;
}

//----------------------------------------------------------------------------
vtkMRMLModelHierarchyLogic* vtkMRMLApplicationLogic::GetModelHierarchyLogic()const
{
  return this->Internal->ModelHierarchyLogic;
}

//----------------------------------------------------------------------------
void vtkMRMLApplicationLogic::SetColorLogic(vtkMRMLColorLogic* colorLogic)
{
  this->Internal->ColorLogic = colorLogic;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkMRMLColorLogic* vtkMRMLApplicationLogic::GetColorLogic()const
{
  return this->Internal->ColorLogic;
}

//----------------------------------------------------------------------------
vtkCollection* vtkMRMLApplicationLogic::GetSliceLogics()const
{
  return this->Internal->SliceLogics;
}

//----------------------------------------------------------------------------
void vtkMRMLApplicationLogic::SetSliceLogics(vtkCollection* sliceLogics)
{
  this->Internal->SliceLogics = sliceLogics;
}

//---------------------------------------------------------------------------
vtkMRMLSliceLogic* vtkMRMLApplicationLogic::
GetSliceLogic(vtkMRMLSliceNode* sliceNode) const
{
  if(!sliceNode || !this->Internal->SliceLogics)
    {
    return 0;
    }

  vtkMRMLSliceLogic* logic = 0;
  vtkCollectionSimpleIterator it;
  vtkCollection* logics = this->Internal->SliceLogics;

  for (logics->InitTraversal(it);
      (logic=vtkMRMLSliceLogic::SafeDownCast(logics->GetNextItemAsObject(it)));)
    {
    if (logic->GetSliceNode() == sliceNode)
      {
      break;
      }

    logic = 0;
    }

  return logic;
}

//---------------------------------------------------------------------------
vtkMRMLSliceLogic* vtkMRMLApplicationLogic::
GetSliceLogicByLayoutLabel(const char* layoutLabel) const
{
  if(!layoutLabel || !this->Internal->SliceLogics)
    {
    return 0;
    }

  vtkMRMLSliceLogic* logic = 0;
  vtkCollectionSimpleIterator it;
  vtkCollection* logics = this->Internal->SliceLogics;

  for (logics->InitTraversal(it);
      (logic=vtkMRMLSliceLogic::SafeDownCast(logics->GetNextItemAsObject(it)));)
    {
    if (logic->GetSliceNode())
      {
      if ( !strcmp( logic->GetSliceNode()->GetLayoutName(), layoutLabel) )
        {
        break;
        }
      }

    logic = 0;
    }

  return logic;
}

//---------------------------------------------------------------------------
void vtkMRMLApplicationLogic::SetMRMLSceneInternal(vtkMRMLScene *newScene)
{
  vtkMRMLNode * selectionNode = 0;
  if (newScene)
    {
    // Selection Node
    selectionNode = newScene->GetNthNodeByClass(0, "vtkMRMLSelectionNode");
    if (!selectionNode)
      {
      selectionNode = newScene->AddNode(vtkNew<vtkMRMLSelectionNode>().GetPointer());
      }
    assert(vtkMRMLSelectionNode::SafeDownCast(selectionNode));
    }
  this->SetSelectionNode(vtkMRMLSelectionNode::SafeDownCast(selectionNode));

  vtkMRMLNode * interactionNode = 0;
  if (newScene)
    {
    // Interaction Node
    interactionNode = newScene->GetNthNodeByClass(0, "vtkMRMLInteractionNode");
    if (!interactionNode)
      {
      interactionNode = newScene->AddNode(vtkNew<vtkMRMLInteractionNode>().GetPointer());
      }
    assert(vtkMRMLInteractionNode::SafeDownCast(interactionNode));
    }
  this->SetInteractionNode(vtkMRMLInteractionNode::SafeDownCast(interactionNode));

  this->Superclass::SetMRMLSceneInternal(newScene);

  this->Internal->SliceLinkLogic->SetMRMLScene(newScene);
  this->Internal->ModelHierarchyLogic->SetMRMLScene(newScene);
}

//----------------------------------------------------------------------------
void vtkMRMLApplicationLogic::SetSelectionNode(vtkMRMLSelectionNode *selectionNode)
{
  if (selectionNode == this->Internal->SelectionNode)
    {
    return;
    }
  this->Internal->SelectionNode = selectionNode;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLApplicationLogic::SetInteractionNode(vtkMRMLInteractionNode *interactionNode)
{
  if (interactionNode == this->Internal->InteractionNode)
    {
    return;
    }
  this->Internal->InteractionNode = interactionNode;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLApplicationLogic::PropagateVolumeSelection(int fit)
{
  if ( !this->Internal->SelectionNode || !this->GetMRMLScene() )
    {
    std::cout << this->Internal->SelectionNode << "    " << this->GetMRMLScene() << std::endl;
    return;
    }

  char *ID = this->Internal->SelectionNode->GetActiveVolumeID();
  char *secondID = this->Internal->SelectionNode->GetSecondaryVolumeID();
  char *labelID = this->Internal->SelectionNode->GetActiveLabelVolumeID();

  vtkMRMLSliceCompositeNode *cnode;
  const int nnodes = this->GetMRMLScene()->GetNumberOfNodesByClass("vtkMRMLSliceCompositeNode");
  for (int i = 0; i < nnodes; i++)
    {
    cnode = vtkMRMLSliceCompositeNode::SafeDownCast (
      this->GetMRMLScene()->GetNthNodeByClass( i, "vtkMRMLSliceCompositeNode" ) );
    if(!cnode->GetDoPropagateVolumeSelection())
      {
      continue;
      }
    cnode->SetBackgroundVolumeID( ID );
    cnode->SetForegroundVolumeID( secondID );
    cnode->SetLabelVolumeID( labelID );
    }

  if (fit) {
    this->FitSliceToAll();
  }
}


//----------------------------------------------------------------------------
void vtkMRMLApplicationLogic::FitSliceToAll()
{
  if (this->Internal->SliceLogics.GetPointer() == 0)
    {
    return;
    }
  vtkMRMLSliceLogic* sliceLogic = 0;
  vtkCollectionSimpleIterator it;
  for(this->Internal->SliceLogics->InitTraversal(it);
      (sliceLogic = vtkMRMLSliceLogic::SafeDownCast(
        this->Internal->SliceLogics->GetNextItemAsObject(it)));)
    {
    vtkMRMLSliceNode *sliceNode = sliceLogic->GetSliceNode();
    int *dims = sliceNode->GetDimensions();
    sliceLogic->FitSliceToAll(dims[0], dims[1]);
    }
}

//----------------------------------------------------------------------------
bool vtkMRMLApplicationLogic::Zip(const char *zipFileName, const char *directoryToZip)
{
  // call function in vtkArchive
  return zip(zipFileName, directoryToZip);
}

//----------------------------------------------------------------------------
bool vtkMRMLApplicationLogic::Unzip(const char *zipFileName, const char *destinationDirectory)
{
  // call function in vtkArchive
  return unzip(zipFileName, destinationDirectory);
}

//----------------------------------------------------------------------------
std::string vtkMRMLApplicationLogic::UnpackSlicerDataBundle(const char *sdbFilePath, const char *temporaryDirectory)
{
  if ( !this->Unzip(sdbFilePath, temporaryDirectory) )
    {
    vtkErrorMacro("could not open bundle file");
    return "";
    }

  vtksys::Glob glob;
  glob.RecurseOn();
  glob.RecurseThroughSymlinksOff();
  std::string globPattern(temporaryDirectory);
  if ( !glob.FindFiles( globPattern + "/*.mrml" ) )
    {
    vtkErrorMacro("could not search archive");
    return "";
    }
  std::vector<std::string> files = glob.GetFiles();
  if ( files.size() <= 0 )
    {
    vtkErrorMacro("could not find mrml file in archive");
    return "";
    }

  return( files[0] );
}

//----------------------------------------------------------------------------
bool vtkMRMLApplicationLogic::OpenSlicerDataBundle(const char *sdbFilePath, const char *temporaryDirectory)
{
  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("no scene");
    return NULL;
    }

  std::string mrmlFile = this->UnpackSlicerDataBundle(sdbFilePath, temporaryDirectory);

  if ( mrmlFile.empty() )
    {
    vtkErrorMacro("Could not unpack mrml scene");
    return false;
    }

  this->GetMRMLScene()->SetURL( mrmlFile.c_str() );
  int result = this->GetMRMLScene()->Connect();
  if ( result )
    {
    vtkErrorMacro("Could not connect to scene");
    return false;
    }
  return true;
}

//----------------------------------------------------------------------------
const char *vtkMRMLApplicationLogic::SaveSceneToSlicerDataBundleDirectory(const char *sdbDir, vtkImageData *screenShot)
{
  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("SaveSceneToSlicerDataBundleDirectory: no scene to bundle!");
    return NULL;
    }
  if (!sdbDir)
    {
    vtkErrorMacro("SaveSceneToSlicerDataBundleDirectory: no directory given!");
    return NULL;
    }

  // if the path to the directory is not absolute, return
  if (!vtksys::SystemTools::FileIsFullPath(sdbDir))
    {
    vtkErrorMacro("SaveSceneToSlicerDataBundleDirectory: given directory is not a full path: " << sdbDir);
    return NULL;
    }
  // is it a directory?
  if (!vtksys::SystemTools::FileIsDirectory(sdbDir))
    {
    vtkErrorMacro("SaveSceneToSlicerDataBundleDirectory: given directory name is not actually a directory, try again!" << sdbDir);
    return NULL;
    }
  std::string rootDir = std::string(sdbDir);
  vtkDebugMacro("SaveSceneToSlicerDataBundleDirectory: Using root dir of " << rootDir);
  // need the components to build file names
  std::vector<std::string> rootPathComponents;
  vtksys::SystemTools::SplitPath(rootDir.c_str(), rootPathComponents);

  // remove the directory if it does exist
  if (vtksys::SystemTools::FileExists(rootDir.c_str(), false))
    {
    vtkWarningMacro("SaveSceneToSlicerDataBundleDirectory: removing SDB scene directory " << rootDir.c_str());
    if (!vtksys::SystemTools::RemoveADirectory(rootDir.c_str()))
      {
      vtkErrorMacro("SaveSceneToSlicerDataBundleDirectory: Error removing SDB scene directory " << rootDir.c_str() << ", cannot make a fresh archive.");
      return NULL;
      }
    }
  // create the SDB directory
  if (!vtksys::SystemTools::FileExists(rootDir.c_str(), false))
    {
    if (!vtksys::SystemTools::MakeDirectory(rootDir.c_str()))
      {
      vtkErrorMacro("SaveSceneToSlicerDataBundleDirectory: Unable to make temporary directory " << rootDir);
      return NULL;
      }
    }

    // create a new scene
  vtkSmartPointer<vtkMRMLScene> sdbScene = vtkSmartPointer<vtkMRMLScene>::New();

  sdbScene->SetRootDirectory(rootDir.c_str());

  // put the mrml scene file in the new directory
  std::string urlStr = vtksys::SystemTools::GetFilenameWithoutExtension(rootDir.c_str()) + std::string(".mrml");
  rootPathComponents.push_back(urlStr);
  urlStr =  vtksys::SystemTools::JoinPath(rootPathComponents);
  sdbScene->SetURL(urlStr.c_str());
  rootPathComponents.pop_back();
  vtkDebugMacro("SaveSceneToSlicerDataBundleDirectory: set new scene url to " << sdbScene->GetURL());

  // create a data directory
  std::vector<std::string> pathComponents;
  vtksys::SystemTools::SplitPath(rootDir.c_str(), pathComponents);
  pathComponents.push_back("Data");
  std::string dataDir =  vtksys::SystemTools::JoinPath(pathComponents);
  vtkDebugMacro("SaveSceneToSlicerDataBundleDirectory: using data dir of " << dataDir);

  // create the data dir
  if (!vtksys::SystemTools::FileExists(dataDir.c_str()))
    {
    if (!vtksys::SystemTools::MakeDirectory(dataDir.c_str()))
      {
      vtkErrorMacro("SaveSceneToSlicerDataBundleDirectory: Unable to make data directory " << dataDir);
      return NULL;
      }
    }

  // copy all the nodes into a new scene so can work on it without changing
  // the current scene
  int numNodes = this->GetMRMLScene()->GetNumberOfNodes();
  for (int i = 0; i < numNodes; ++i)
    {
    vtkMRMLNode *mrmlNode = this->GetMRMLScene()->GetNthNode(i);
    if (!mrmlNode)
      {
      vtkErrorMacro("SaveSceneToSlicerDataBundleDirectory: unable to get " << i << "th node from scene with " << numNodes << " nodes");
      break;
      }
    vtkMRMLNode *copyNode = sdbScene->CopyNode(mrmlNode);
    if (!copyNode)
      {
      vtkErrorMacro("SaveSceneToSlicerDataBundleDirectory: unable to make a copy of node " << i << " with id " << (mrmlNode->GetID() ? mrmlNode->GetID() : "NULL"));
      }
    else
      {
      if (copyNode->IsA("vtkMRMLStorableNode"))
        {
        // adjust the file paths
        vtkMRMLStorableNode *storableNode = vtkMRMLStorableNode::SafeDownCast(copyNode);
        if (storableNode && storableNode->GetSaveWithScene())
          {
          vtkMRMLStorageNode *snode = storableNode->GetStorageNode();
          if (!snode)
            {
            // have to create one!
            vtkWarningMacro("SaveSceneToSlicerDataBundleDirectory: creating a new storage node for " << storableNode->GetID());
            snode = storableNode->CreateDefaultStorageNode();
            if (snode)
              {
              std::string storageFileName = std::string(storableNode->GetName()) + std::string(".") + std::string(snode->GetDefaultWriteFileExtension());
              vtkDebugMacro("SaveSceneToSlicerDataBundleDirectory: new file name = " << storageFileName.c_str());
              snode->SetFileName(storageFileName.c_str());
              sdbScene->AddNode(snode);
              storableNode->SetAndObserveStorageNodeID(snode->GetID());
              snode->Delete();
              snode = storableNode->GetStorageNode();
              }
            else
              {
              vtkErrorMacro("SaveSceneToSlicerDataBundleDirectory: cannot make a new storage node for storable node " << storableNode->GetID());
              }
            }
          if (snode)
            {
            snode->SetDataDirectory(dataDir.c_str());
            vtkDebugMacro("SaveSceneToSlicerDataBundleDirectory: set data directory to " << dataDir.c_str() << ", storable node " << storableNode->GetID() << " file name is now: " << snode->GetFileName());
            // deal with existing files
            if (vtksys::SystemTools::FileExists(snode->GetFileName(), true))
              {
              vtkWarningMacro("SaveSceneToSlicerDataBundleDirectory: file " << snode->GetFileName() << " already exists, renaming!");
              std::string baseName = vtksys::SystemTools::GetFilenameWithoutExtension(snode->GetFileName());
              std::string ext = vtksys::SystemTools::GetFilenameExtension(snode->GetFileName());
              bool uniqueName = false;
              std::string uniqueFileName;
              int v = 1;
              while (!uniqueName)
                {
                std::stringstream ss;
                ss << v;
                uniqueFileName = baseName + ss.str() + ext;
                if (vtksys::SystemTools::FileExists(uniqueFileName.c_str()) == 0)
                  {
                  uniqueName = true;
                  }
                else
                  {
                  ++v;
                  }
                }
              vtkDebugMacro("SaveSceneToSlicerDataBundleDirectory: found unique file name " << uniqueFileName.c_str());
              snode->SetFileName(uniqueFileName.c_str());

              }
            snode->WriteData(storableNode);
            }
          }
        }
      }
    }
  // create a scene view, using the snapshot passed in if any
  vtkMRMLSceneViewNode * newSceneViewNode = vtkMRMLSceneViewNode::New();
  newSceneViewNode->SetScene(sdbScene);
  newSceneViewNode->SetName(sdbScene->GetUniqueNameByString("Slicer Data Bundle Scene View"));
  newSceneViewNode->SetSceneViewDescription("Scene at MRML file save point");
  if (screenShot)
    {
    // assumes has been passed a screen shot of the full layout
    newSceneViewNode->SetScreenShotType(4);
    newSceneViewNode->SetScreenShot(screenShot);
    // mark it modified since read so that the screen shot will get saved to disk
    newSceneViewNode->ModifiedSinceReadOn();
    }
  // save the scene view
  newSceneViewNode->StoreScene();
  sdbScene->AddNode(newSceneViewNode);
  // create a storage node
  vtkMRMLStorageNode *storageNode = newSceneViewNode->CreateDefaultStorageNode();
  // set the file name from the node name, using a relative path, it will go
  // at the same level as the  .mrml file
  std::string sceneViewFileName = std::string(newSceneViewNode->GetName()) + std::string(".png");
  storageNode->SetFileName(sceneViewFileName.c_str());
  sdbScene->AddNode(storageNode);
  newSceneViewNode->SetAndObserveStorageNodeID(storageNode->GetID());
  // force a write
  storageNode->WriteData(newSceneViewNode);
  // clean up
  newSceneViewNode->Delete();
  storageNode->Delete();

  // write the scene to disk, changes paths to relative
  vtkDebugMacro("SaveSceneToSlicerDataBundleDirectory: calling commit on the scene, to url " << sdbScene->GetURL());
  sdbScene->Commit();

  return sdbScene->GetURL();
}

//----------------------------------------------------------------------------
int vtkMRMLApplicationLogic::LoadDefaultParameterSets(vtkMRMLScene *scene,
                                                      const std::vector<std::string>& directories)
{

  // build up the vector
  std::vector<std::string> filesVector;
  std::vector<std::string> filesToLoad;
  //filesVector.push_back(""); // for relative path

// Didn't port this next block of code yet.  Would need to add a
//   UserParameterSetsPath to the object and some window
//
//   // add the list of dirs set from the application
//   if (this->UserColorFilePaths != NULL)
//     {
//     vtkDebugMacro("\nFindColorFiles: got user color file paths = " << this->UserColorFilePaths);
//     // parse out the list, breaking at delimiter strings
// #ifdef WIN32
//     const char *delim = ";";
// #else
//     const char *delim = ":";
// #endif
//     char *ptr = strtok(this->UserColorFilePaths, delim);
//     while (ptr != NULL)
//       {
//       std::string dir = std::string(ptr);
//       vtkDebugMacro("\nFindColorFiles: Adding user dir " << dir.c_str() << " to the directories to check");
//       DirectoriesToCheck.push_back(dir);
//       ptr = strtok(NULL, delim);
//       }
//     } else { vtkDebugMacro("\nFindColorFiles: oops, the user color file paths aren't set!"); }


  // Get the list of parameter sets in these dir
  for (unsigned int d = 0; d < directories.size(); d++)
    {
    std::string dirString = directories[d];
    //vtkDebugMacro("\nLoadDefaultParameterSets: checking for parameter sets in dir " << d << " = " << dirString.c_str());

    filesVector.clear();
    filesVector.push_back(dirString);
    filesVector.push_back(std::string("/"));

#ifdef WIN32
    WIN32_FIND_DATA findData;
    HANDLE fileHandle;
    int flag = 1;
    std::string search ("*.*");
    dirString += "/";
    search = dirString + search;

    fileHandle = FindFirstFile(search.c_str(), &findData);
    if (fileHandle != INVALID_HANDLE_VALUE)
      {
      while (flag)
        {
        // add this file to the vector holding the base dir name so check the
        // file type using the full path
        filesVector.push_back(std::string(findData.cFileName));
#else
    DIR *dp;
    struct dirent *dirp;
    if ((dp  = opendir(dirString.c_str())) == NULL)
      {
      vtkGenericWarningMacro("Error(" << errno << ") opening " << dirString.c_str());
      }
    else
      {
      while ((dirp = readdir(dp)) != NULL)
        {
        // add this file to the vector holding the base dir name
        filesVector.push_back(std::string(dirp->d_name));
#endif

        std::string fileToCheck = vtksys::SystemTools::JoinPath(filesVector);
        int fileType = vtksys::SystemTools::DetectFileType(fileToCheck.c_str());
        if (fileType == vtksys::SystemTools::FileTypeText)
          {
          //vtkDebugMacro("\nAdding " << fileToCheck.c_str() << " to list of potential parameter sets. Type = " << fileType);
          filesToLoad.push_back(fileToCheck);
          }
        else
          {
          //vtkDebugMacro("\nSkipping potential parameter set " << fileToCheck.c_str() << ", file type = " << fileType);
          }
        // take this file off so that can build the next file name
        filesVector.pop_back();

#ifdef WIN32
        flag = FindNextFile(fileHandle, &findData);
        } // end of while flag
      FindClose(fileHandle);
      } // end of having a valid fileHandle
#else
        } // end of while loop over reading the directory entries
      closedir(dp);
      } // end of able to open dir
#endif

    } // end of looping over dirs

  // Save the URL and root directory of the scene so it can
  // be restored after loading presets
  std::string url = scene->GetURL();
  std::string rootdir = scene->GetRootDirectory();

  // Finally, load each of the parameter sets
  std::vector<std::string>::iterator fit;
  for (fit = filesToLoad.begin(); fit != filesToLoad.end(); ++fit)
    {
    scene->SetURL( (*fit).c_str() );
    scene->Import();
    }

  // restore URL and root dir
  scene->SetURL(url.c_str());
  scene->SetRootDirectory(rootdir.c_str());

  return static_cast<int>(filesToLoad.size());
}
