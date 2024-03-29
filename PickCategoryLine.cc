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
#include "package_meta.h"
#include "resource.h"
#include <sstream>

const std::wstring
PickCategoryLine::get_text (int col_num) const
{
  if (col_num == pkgname_col)
    {
      std::ostringstream s;
      s << cat_tree->category().first;
      if (pkgcount)
        s << " (" << pkgcount << ")";
      return string_to_wstring(s.str());
    }
  else if (col_num == new_col)
    {
      return LoadStringW(packagemeta::action_caption (cat_tree->action()));
    }
  return L"";
}

int
PickCategoryLine::do_action(int col_num, int action_id)
{
  if (col_num == pkgname_col)
    {
      cat_tree->collapsed() = ! cat_tree->collapsed();
      theView.refresh();
    }
  else if (col_num == new_col)
    {
      theView.GetParent ()->SetBusy ();
      int u = cat_tree->do_action((packagemeta::_actions)(action_id), theView.deftrust);
      theView.GetParent ()->ClearBusy ();
      return u;
    }
  return 1;
}

int
PickCategoryLine::do_default_action(int col_num)
{
  return 0;
}

ActionList *
PickCategoryLine::get_actions(int col) const
{
  ActionList *al = new ActionList();
  packagemeta::_actions current_default = cat_tree->action();

  al->add(IDS_ACTION_DEFAULT, (int)packagemeta::NoChange_action, (current_default == packagemeta::NoChange_action), TRUE);
  al->add(IDS_ACTION_INSTALL, (int)packagemeta::Install_action, (current_default == packagemeta::Install_action), TRUE);
  al->add(packagedb::task == PackageDB_Install ? IDS_ACTION_REINSTALL : IDS_ACTION_RETRIEVE,
          (int)packagemeta::Reinstall_action, (current_default == packagemeta::Reinstall_action), TRUE);
  al->add(IDS_ACTION_UNINSTALL, (int)packagemeta::Uninstall_action, (current_default == packagemeta::Uninstall_action), TRUE);

  return al;
}

ListViewLine::State
PickCategoryLine::get_state() const
{
  return cat_tree->collapsed() ? State::collapsed : State::expanded;
}

int
PickCategoryLine::get_indent() const
{
  return indent;
}

const std::string
PickCategoryLine::get_tooltip(int col_num) const
{
  return "";
}

int
PickCategoryLine::map_key_to_action(WORD vkey, int modkeys, int & col_num,
                                    int & action_id) const
{
  switch (vkey)
    {
    case VK_SPACE: // expand <> collapse category
      col_num = pkgname_col;
      return Action::Direct;
    case VK_APPS: // install/reinstall/uninstall context menu for category
      col_num = new_col;
      return Action::PopUp;
    }

  return Action::None;
}
