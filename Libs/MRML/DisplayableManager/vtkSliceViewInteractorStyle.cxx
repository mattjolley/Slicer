/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkSliceViewInteractorStyle.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/


#include "vtkSliceViewInteractorStyle.h"

// VTK includes
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderWindow.h"
#include "vtkRenderer.h"
#include "vtkObjectFactory.h"
#include "vtkCommand.h"
#include "vtkNew.h"

// MRML includes
#include "vtkMRMLSliceLogic.h"
#include "vtkMRMLSliceNode.h"
#include "vtkMRMLSliceCompositeNode.h"

//----------------------------------------------------------------------------
vtkCxxSetObjectMacro(vtkSliceViewInteractorStyle, SliceLogic, vtkMRMLSliceLogic);

//----------------------------------------------------------------------------
vtkCxxRevisionMacro(vtkSliceViewInteractorStyle, "$Revision$");
vtkStandardNewMacro(vtkSliceViewInteractorStyle);

//----------------------------------------------------------------------------
vtkSliceViewInteractorStyle::vtkSliceViewInteractorStyle()
{
  this->ActionState = vtkSliceViewInteractorStyle::None;

  this->ActionStartSliceToRAS = vtkMatrix4x4::New();
  this->ActionStartXYToRAS = vtkMatrix4x4::New();
  this->ScratchMatrix = vtkMatrix4x4::New();

  this->ActionStartForegroundOpacity = 0;
  this->ActionStartLabelOpacity = 0;

  this->SliceLogic = 0;
}

//----------------------------------------------------------------------------
vtkSliceViewInteractorStyle::~vtkSliceViewInteractorStyle() 
{
  this->ActionStartSliceToRAS->Delete();
  this->ActionStartXYToRAS->Delete();
  this->ScratchMatrix ->Delete();

  this->SetSliceLogic(0);
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::PrintSelf(ostream& os, vtkIndent indent) 
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "\nSlice Logic:\n";
  if (this->SliceLogic)
    {
    this->SliceLogic->PrintSelf(os, indent.GetNextIndent());
    }
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnKeyPress()
{
  char *key = this->Interactor->GetKeySym();

  // Note: up/down/left/right keys don't come via OnChar, so handle
  // them in OnKeyPress instead
  if ( !strcmp(key, "Right") || !strcmp(key, "Up") )
    {
    this->IncrementSlice();
    }
  else if ( !strcmp(key, "Left") || !strcmp(key, "Down") )
    {
    this->DecrementSlice();
    }
  else
    {
    this->Superclass::OnKeyPress();
    }
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnKeyRelease()
{
  this->Superclass::OnKeyRelease();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnChar() 
{
  vtkMRMLSliceNode *sliceNode = this->SliceLogic->GetSliceNode();
  vtkMRMLSliceCompositeNode *sliceCompositeNode = this->SliceLogic->GetSliceCompositeNode();

  char *key = this->Interactor->GetKeySym();

  if ( !strcmp(key, "f") )
    {
    this->IncrementSlice();
    }
  else if ( !strcmp(key, "b") )
    {
    this->DecrementSlice();
    }
  else if ( !strcmp(key, "v") )
    {
    sliceNode->SetSliceVisible( ! sliceNode->GetSliceVisible() );
    }
  else if ( !strcmp(key, "V") )
    {
    // TODO: need to set all slices visible
    sliceNode->SetSliceVisible( ! sliceNode->GetSliceVisible() );
    }
  else if ( !strcmp(key, "r") )
    {
    this->SliceLogic->StartSliceNodeInteraction(vtkMRMLSliceNode::ResetFieldOfViewFlag);
    this->SliceLogic->FitSliceToAll();
    sliceNode->UpdateMatrices();
    this->SliceLogic->EndSliceNodeInteraction();
    }
  else if ( !strcmp(key, "g") )
    {
    double opacity = sliceCompositeNode->GetLabelOpacity();
    if ( opacity != 0.0 )
      {
      this->SetActionStartLabelOpacity(opacity);
      sliceCompositeNode->SetLabelOpacity(0.0);
      }
    else
      {
      sliceCompositeNode->SetLabelOpacity(this->GetActionStartLabelOpacity());
      }
    }
  else if ( !strcmp(key, "t") )
    {
    double opacity = sliceCompositeNode->GetForegroundOpacity();
    if ( opacity != 0.0 )
      {
      this->SetActionStartForegroundOpacity(opacity);
      sliceCompositeNode->SetForegroundOpacity(0.0);
      }
    else
      {
      sliceCompositeNode->SetForegroundOpacity(this->GetActionStartForegroundOpacity());
      }
    }
  else if ( !strcmp(key, "s") )
    {
    vtkErrorMacro("TODO: set active lightbox/compare view slice");
    /*
     * This is commented out until the active slice highlight and the 
     * correct texture is assigned.  See but #1644
    double xyz[4];
    this->GetEventXYZ(xyz);
    int k = static_cast<int>(xyz[2] + 0.5);
    if ( k >= 0 && k < sliceNode->GetDimensions()[2] )
      {
      sliceNode->SetActiveSlice(k);
      }
    */
    }
  else if ( !strcmp(key, "S") )
    {
    vtkErrorMacro("TODO: set active lightbox/compare view slice");
    }
  else
    {
    this->Superclass::OnChar();
    }
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnRightButtonDown() 
{
  this->SliceLogic->GetMRMLScene()->SaveStateForUndo(this->SliceLogic->GetSliceNode());
  this->SetActionState(vtkSliceViewInteractorStyle::Zoom);
  this->SliceLogic->StartSliceNodeInteraction(vtkMRMLSliceNode::FieldOfViewFlag);
  vtkMRMLSliceNode *sliceNode = this->SliceLogic->GetSliceNode();
  this->SetActionStartFOV(sliceNode->GetFieldOfView());
  this->SetActionStartWindow(this->GetInteractor()->GetEventPosition());
}
//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnRightButtonUp() 
{
  this->SetActionState(vtkSliceViewInteractorStyle::None);
  this->SliceLogic->EndSliceNodeInteraction();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnMiddleButtonDown() 
{
  this->StartTranslate();
  this->SetActionStartWindow(this->GetInteractor()->GetEventPosition());
}
//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnMiddleButtonUp() 
{
  this->EndTranslate();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnLeftButtonDown() 
{
  if (this->Interactor->GetShiftKey())
    {
    this->StartTranslate();
    }
  else if (this->Interactor->GetControlKey())
    {
    this->StartBlend();
    }
  this->SetActionStartWindow(this->GetInteractor()->GetEventPosition());
  this->Superclass::OnLeftButtonDown();
}
//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnLeftButtonUp() 
{
  if (this->ActionState == this->Translate)
    {
    this->EndTranslate();
    }
  else if (this->ActionState == this->Blend)
    {
    this->EndBlend();
    }
  this->Superclass::OnLeftButtonUp();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnMouseMove() 
{
  vtkMRMLSliceNode *sliceNode = this->SliceLogic->GetSliceNode();
  vtkMRMLSliceCompositeNode *sliceCompositeNode = this->SliceLogic->GetSliceCompositeNode();
  int windowX, windowY;
  this->GetInteractor()->GetEventPosition(windowX, windowY);
  int windowW = this->GetInteractor()->GetRenderWindow()->GetSize()[0];
  int windowH = this->GetInteractor()->GetRenderWindow()->GetSize()[1];
  int windowMinSize = std::min(windowW, windowH);

  switch (this->GetActionState())
    {
    case vtkSliceViewInteractorStyle::Translate:
      {
      sliceNode->SetSliceOrigin(this->ActionStartWindow[0] - windowX,
                                this->ActionStartWindow[1] - windowY, 0);
      }
      break;
    case vtkSliceViewInteractorStyle::Zoom:
      {
      int deltaY = windowY - this->GetActionStartWindow()[1];
      double percent = (windowH + deltaY) / (1.0 * windowH);

      // the factor operation is so 'z' isn't changed and the 
      // slider can still move through the full range
      if ( percent > 0. ) 
        {
        double newFOVx = this->GetActionStartFOV()[0] * percent;
        double newFOVy = this->GetActionStartFOV()[1] * percent;
        double newFOVz = this->GetActionStartFOV()[2];
        sliceNode->SetFieldOfView( newFOVx, newFOVy, newFOVz );
        }
      }
      break;
    case vtkSliceViewInteractorStyle::Blend:
      {
      int deltaY = windowY - this->GetActionStartWindow()[1];
      double offsetY =  (2.0 * deltaY) / windowMinSize;
      double newForegroundOpacity =
        this->GetActionStartForegroundOpacity() + offsetY;
      newForegroundOpacity = std::min(std::max(newForegroundOpacity, 0.), 1.);
      if (sliceCompositeNode->GetForegroundVolumeID() != 0)
        {
        sliceCompositeNode->SetForegroundOpacity(newForegroundOpacity);
        }
      int deltaX = windowX - this->GetActionStartWindow()[0];
      double offsetX =  (2.0 * deltaX) / windowMinSize;
      double newLabelOpacity = this->GetActionStartLabelOpacity() + offsetX;
      newLabelOpacity = std::min(std::max(newLabelOpacity, 0.), 1.);
      if (sliceCompositeNode->GetLabelVolumeID() != 0)
        {
        sliceCompositeNode->SetLabelOpacity(newLabelOpacity);
        }
      }
      break;
    default:
      {
      if (this->Interactor->GetShiftKey())
        {
        double eventRAS[4];
        this->GetEventRAS(eventRAS);
        sliceNode->JumpAllSlices(eventRAS[0],eventRAS[1],eventRAS[2]);
        }
      else
        {
        this->Superclass::OnMouseMove();
        }
      }
    }
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnMouseWheelForward() 
{
  this->IncrementSlice();
  this->Superclass::OnMouseWheelForward();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnMouseWheelBackward() 
{
  this->DecrementSlice();
  this->Superclass::OnMouseWheelBackward();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnExpose()
{
  this->Superclass::OnExpose();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnConfigure() 
{
  this->Superclass::OnConfigure();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnEnter()
{
  this->Superclass::OnEnter();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::OnLeave()
{
  this->Superclass::OnLeave();
}

//----------------------------------------------------------------------------
double vtkSliceViewInteractorStyle::GetSliceSpacing()
{
  double spacing;
  vtkMRMLSliceNode *sliceNode = this->SliceLogic->GetSliceNode();

  if (sliceNode->GetSliceSpacingMode() == vtkMRMLSliceNode::PrescribedSliceSpacingMode)
    {
    spacing = sliceNode->GetPrescribedSliceSpacing()[2];
    }
  else
    {
    spacing = this->SliceLogic->GetLowestVolumeSliceSpacing()[2];
    }
  return spacing;
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::IncrementSlice()
{
  this->MoveSlice(this->GetSliceSpacing());
}
//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::DecrementSlice()
{
  this->MoveSlice(-1. * this->GetSliceSpacing());
}
//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::MoveSlice(double delta)
{
  double offset = this->SliceLogic->GetSliceOffset();

  this->SliceLogic->StartSliceNodeInteraction(vtkMRMLSliceNode::SliceToRASFlag);
  this->SliceLogic->SetSliceOffset(offset + delta);
  this->SliceLogic->EndSliceNodeInteraction();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::StartTranslate()
{
  vtkMRMLSliceNode *sliceNode = this->SliceLogic->GetSliceNode();
  this->SliceLogic->GetMRMLScene()->SaveStateForUndo(sliceNode);
  this->SliceLogic->StartSliceNodeInteraction(vtkMRMLSliceNode::SliceToRASFlag);

  this->SetActionState(this->Translate);
  this->GetActionStartSliceToRAS()->DeepCopy(sliceNode->GetSliceToRAS());
  this->GetActionStartXYToRAS()->DeepCopy(sliceNode->GetXYToRAS());
  double eventRAS[4];
  this->GetEventRAS(eventRAS);
  this->SetActionStartRAS(eventRAS);
}
//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::EndTranslate()
{
  this->SetActionState(this->None);
  this->SliceLogic->EndSliceNodeInteraction();
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::StartBlend()
{
  this->SetActionState(this->Blend);
  vtkMRMLSliceCompositeNode *sliceCompositeNode = this->SliceLogic->GetSliceCompositeNode();
  this->SetActionStartForegroundOpacity(sliceCompositeNode->GetForegroundOpacity());
  this->SetActionStartLabelOpacity(sliceCompositeNode->GetLabelOpacity());
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::EndBlend()
{
  this->SetActionState(this->None);
  vtkMRMLSliceCompositeNode *sliceCompositeNode = this->SliceLogic->GetSliceCompositeNode();
  this->SetActionStartForegroundOpacity(sliceCompositeNode->GetForegroundOpacity());
  this->SetActionStartLabelOpacity(sliceCompositeNode->GetLabelOpacity());
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::GetEventRAS(double ras[4])
{
  int windowX, windowY;

  vtkMRMLSliceNode *sliceNode = this->SliceLogic->GetSliceNode();
  this->GetInteractor()->GetEventPosition(windowX, windowY);
  vtkRenderer *pokedRenderer = this->GetInteractor()->FindPokedRenderer(windowX, windowY);

  double localXY[4];
  localXY[0] = windowX - pokedRenderer->GetOrigin()[0];
  localXY[1] = windowY - pokedRenderer->GetOrigin()[1];
  localXY[2] = 0.;
  localXY[3] = 1.;
  // map the current point from XY to RAS space
  sliceNode->GetXYToRAS()->MultiplyPoint(localXY, ras);
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::GetEventRASWithRespectToEventStart(double ras[4])
{
  int windowX, windowY;

  this->GetInteractor()->GetEventPosition(windowX, windowY);
  vtkRenderer *pokedRenderer = this->GetInteractor()->FindPokedRenderer(windowX, windowY);

  double localXY[4];
  localXY[0] = windowX - pokedRenderer->GetOrigin()[0];
  localXY[1] = windowY - pokedRenderer->GetOrigin()[1];
  localXY[2] = 0.;
  localXY[3] = 1.;
  // map the current point from XY to RAS space
  this->GetActionStartXYToRAS()->MultiplyPoint(localXY, ras);
}

//----------------------------------------------------------------------------
void vtkSliceViewInteractorStyle::GetEventXYZ(double xyz[4])
{
  int windowX, windowY;
  int windowW, windowH;
  this->GetInteractor()->GetEventPosition(windowX, windowY);

  vtkMRMLSliceNode *sliceNode = this->SliceLogic->GetSliceNode();

  int *size = this->GetInteractor()->GetRenderWindow()->GetSize();
  windowW = size[0];
  windowH = size[1];
  int numRows = sliceNode->GetLayoutGridRows();
  int numCols = sliceNode->GetLayoutGridColumns();

  if ( windowW == 0 || windowH == 0 )
    {
    // degenerate case, return gracefully
    xyz[0] = xyz[1] = xyz[2] = 0.;
    xyz[3] = 1.;
    return;
    }

  vtkRenderer *pokedRenderer = this->GetInteractor()->FindPokedRenderer(windowX, windowY);
  int *origin = pokedRenderer->GetOrigin();
  xyz[0] = windowX - origin[0];
  xyz[1] = windowY - origin[1];

  double tx = windowX / (1. * windowW);
  double ty = (windowH - windowY) / (1. * windowH);
  xyz[2] = (floor(ty*numRows)*numCols + floor(tx*numCols));
  
  xyz[3] = 1.;
}
