/*
 * Copyright (c) 2002 Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <robertc@hotmail.com>
 *
 */

#ifndef _PICKLINE_H_
#define _PICKLINE_H_

#include "win32.h"
#include "package_meta.h"

class PickLine
{
public:
  virtual void paint (HDC hdc, int x, int y, int row, int show_cat) = 0;
  virtual int click (int const myrow, int const ClickedRow, int const x) = 0;
  virtual int set_action (packagemeta::_actions) = 0;
  virtual int itemcount () const = 0;
  // this may indicate bad inheritance model.
  virtual bool IsContainer (void) const = 0;
  virtual void insert (PickLine &) = 0;
  // Never allocate to key, always allocated elsewhere
  char const *key;
  virtual ~ PickLine ()
  {
  };
protected:
  PickLine ()
  {
  };
  PickLine (char const *aKey):key (aKey)
  {
  };
  PickLine (PickLine const &);
  PickLine & operator= (PickLine const &);
};

#endif // _PICKLINE_H_
