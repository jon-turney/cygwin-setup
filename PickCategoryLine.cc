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

#include "PickCategoryLine.h"
#include "package_db.h"
#include "PickView.h"
#include "window.h"

const std::string
PickCategoryLine::get_text (int col_num) const
{
  if (col_num == pkgname_col)
    {
      std::string s = (cat_tree->collapsed() ? "[+] " : "[-] ") + cat_tree->category().first;
      return s;
    }
  else if (col_num == new_col)
    {
      return packagemeta::action_caption (cat_tree->action());
    }
  return "";
}

int
PickCategoryLine::do_action(int col_num)
{
  if (col_num == pkgname_col)
    {
      cat_tree->collapsed() = ! cat_tree->collapsed();
      theView.refresh();
    }
  else if (col_num == new_col)
    {
      theView.GetParent ()->SetBusy ();
      int u = cat_tree->do_action((packagemeta::_actions)((cat_tree->action() + 1) % 4), theView.deftrust);
      theView.GetParent ()->ClearBusy ();
      return u;
    }
  return 1;
}
