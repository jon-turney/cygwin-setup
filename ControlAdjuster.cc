/*
 * Copyright (c) 2003, Frank Richter <frichter@gmx.li>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Frank Richter.
 *
 */

#include "ControlAdjuster.h"
#include "RECTWrapper.h"
 
void ControlAdjuster::AdjustControls (HWND dlg, 
				      const ControlInfo controlInfo[],   
				      int widthChange, int heightChange)
{
  const ControlInfo* ci = controlInfo;
  
  while (ci->control > 0)
  {
    HWND ctl = GetDlgItem (dlg, ci->control);  
    if (ctl != 0)
    {
      RECTWrapper ctlRect;
      GetWindowRect (ctl, &ctlRect);
      // We want client coords.
      ScreenToClient (dlg, (LPPOINT)&ctlRect.left);
      ScreenToClient (dlg, (LPPOINT)&ctlRect.right);
      
      /*
        Now adjust the rectangle.
       */
      switch (ci->horizontalPos)
      {
	case CP_LEFT:
	  break;
	case CP_MIDDLE:
	  ctlRect.left += widthChange/2;
	  ctlRect.right += widthChange - widthChange/2;
	  break;
	case CP_RIGHT:
	  ctlRect.left += widthChange;
	  ctlRect.right += widthChange;
	  break;
	case CP_STRETCH:
	  ctlRect.right += widthChange;
	  break;
      }
      switch (ci->verticalPos)
      {
	case CP_TOP:
	  break;
	case CP_MIDDLE:
	  ctlRect.top += heightChange/2;
	  ctlRect.bottom += heightChange - heightChange/2;
	  break;
	case CP_BOTTOM:
	  ctlRect.top += heightChange;
	  ctlRect.bottom += heightChange;
	  break;
	case CP_STRETCH:
	  ctlRect.bottom += heightChange;
	  break;
      }

      SetWindowPos (ctl, 0, ctlRect.left, ctlRect.top, 
	ctlRect.width (), ctlRect.height (), SWP_NOACTIVATE | SWP_NOZORDER);
      // If not done, weird visual glitches can occur.
      InvalidateRect (ctl, 0, false);
      
    }
    ci++;
  }
}
 
SizeProcessor::SizeProcessor ()
{
  rectValid = false;
}
 
void SizeProcessor::AddControlInfo (
  const ControlAdjuster::ControlInfo* controlInfo)
{
  controlInfos.push_back (controlInfo);
}

void SizeProcessor::UpdateSize (HWND dlg)
{
  RECTWrapper clientRect;
  ::GetClientRect (dlg, &clientRect);

  if (rectValid)
    {
      const int dX = clientRect.width () - lastRect.width ();
      const int dY = clientRect.height () - lastRect.height ();
	
      for (size_t i = 0; i < controlInfos.size (); i++)
	ControlAdjuster::AdjustControls (dlg, controlInfos[i], dX, dY);
    }
  else
    rectValid = true;
    
  lastRect = clientRect;
}
