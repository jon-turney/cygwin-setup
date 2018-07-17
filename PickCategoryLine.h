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

#ifndef SETUP_PICKCATEGORYLINE_H
#define SETUP_PICKCATEGORYLINE_H

#include "package_meta.h"
#include "ListView.h"
#include "PickView.h"

class PickCategoryLine: public ListViewLine
{
public:
  PickCategoryLine (PickView & aView, CategoryTree * _tree, int _pkgcount) :
    cat_tree (_tree),
    pkgcount(_pkgcount),
    theView (aView)
  {
  };
  ~PickCategoryLine ()
  {
  }

  const std::string get_text(int col) const;
  State get_state() const;
  ActionList *get_actions(int col) const;
  int do_action(int col, int action_id);

private:
  CategoryTree * cat_tree;
  int pkgcount;
  PickView & theView;
};

#endif /* SETUP_PICKCATEGORYLINE_H */
