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

#ifndef SETUP_PICKPACKAGELINE_H
#define SETUP_PICKPACKAGELINE_H

class PickView;

#include "package_meta.h"
#include "ListView.h"

class PickPackageLine: public ListViewLine
{
public:
  PickPackageLine (PickView &aView, packagemeta & apkg, int aindent) :
    pkg (apkg),
    theView (aView),
    indent (aindent)
  {
  };
  const std::wstring get_text(int col) const;
  State get_state() const { return State::nothing; }
  const std::string get_tooltip(int col) const;
  int get_indent() const;
  ActionList *get_actions(int col_num) const;
  int do_action(int col, int action_id);
  int do_default_action(int col);
  int map_key_to_action(WORD vkey, int modkeys, int & col_num, int & action_id) const;
private:
  packagemeta & pkg;
  PickView & theView;
  int indent;
};

#endif /* SETUP_PICKPACKAGELINE_H */
