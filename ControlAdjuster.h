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

#ifndef SETUP_CONTROLADJUSTER_H
#define SETUP_CONTROLADJUSTER_H

#include <vector>

#include <windows.h>
#include "RECTWrapper.h"

/*
  This is a helper class to move/resize controls of a dialog when it's size
  is changed. It's no fancy layouting stuff, but rather just moving them
  around - to, for example, keep controls at the bottom really at the bottom
  when the size changes.
 */

enum ControlPosition {
  CP_LEFT = 0,
  CP_TOP = CP_LEFT,
  CP_MIDDLE,
  CP_RIGHT,
  CP_BOTTOM = CP_RIGHT,
  CP_STRETCH,
};

class ControlAdjuster
{
public:
  struct ControlInfo
  {
    // Control ID
    int control;
    /*
     * Position specifiers.
     */
    ControlPosition horizontalPos;
    ControlPosition verticalPos;
  };
  
  /*
    Adjust all the controls.
    'controlInfo' an array with the moving information.
    The terminating item of the array should have an ID <= 0.
   */
  static void AdjustControls (HWND dlg, const ControlInfo controlInfo[],
    int widthChange, int heightChange);
};

class SizeProcessor
{
  typedef std::vector<const ControlAdjuster::ControlInfo*> ControlInfos;
  ControlInfos controlInfos;
  bool rectValid;
  RECTWrapper lastRect;
public:
  SizeProcessor ();
  
  void AddControlInfo (const ControlAdjuster::ControlInfo* controlInfo);
  void UpdateSize (HWND dlg);
};

#endif // SETUP_CONTROLADJUSTER_H 
