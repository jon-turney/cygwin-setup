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

#include "PickPackageLine.h"
#include "PickView.h"
#include "package_db.h"

const std::wstring
PickPackageLine::get_text(int col_num) const
{
  if (col_num == pkgname_col)
    {
      return string_to_wstring(pkg.name);
    }
  else if (col_num == current_col)
    {
      return string_to_wstring(pkg.installed.Canonical_version());
    }
  else if (col_num == new_col)
    {
      return pkg.action_caption ();
    }
  else if (col_num == srctick_col)
    {
      const char *srctick = "?";
      if ( /* uninstall */ !pkg.desired ||
           /* when no source mirror available */
           !pkg.desired.sourcePackage().accessible())
        srctick = "n/a";
      else if (pkg.srcpicked())
        srctick = "yes";
      else
        srctick = "no";

      return string_to_wstring(srctick);
    }
  else if (col_num == cat_col)
    {
      return string_to_wstring(pkg.getReadableCategoryList());
    }
  else if (col_num == size_col)
    {
      int sz = 0;
      packageversion picked;

      /* Find the size of the package.  If user has chosen to upgrade/downgrade
         the package, use that version.  Otherwise use the currently installed
         version, or if not installed then use the version that would be chosen
         based on the current trust level (curr/prev/test).  */
      if (pkg.desired)
        picked = pkg.desired;
      else if (pkg.installed)
        picked = pkg.installed;
      else
        picked = pkg.trustp (false, theView.deftrust);

      /* Include the size of the binary package, and if selected, the source
         package as well.  */
      sz += picked.source()->size;
      if (pkg.srcpicked())
        sz += picked.sourcePackage().source()->size;

      /* If size still 0, size must be unknown.  */
      std::string size = (sz == 0) ? "?" : format_1000s((sz+1023)/1024) + "k";

      return string_to_wstring(size);
    }
  else if (col_num == pkg_col)
    {
      return string_to_wstring(pkg.SDesc());
    }

  return L"unknown";
}

const std::string
PickPackageLine::get_tooltip(int col_num) const
{
  if (col_num == pkg_col)
    {
      return pkg.LDesc();
    }

  return "";
}

int
PickPackageLine::do_action(int col_num, int action_id)
{
  if (col_num == new_col)
    {
      pkg.select_action(action_id, theView.deftrust);
      return 1;
    }

  if (col_num == srctick_col)
    {
      if (pkg.desired.sourcePackage ().accessible ())
        pkg.srcpick (!pkg.srcpicked ());

      return 1;
    }

  return 0;
}

int
PickPackageLine::do_default_action(int col_num)
{
  if (col_num == new_col)
    {
      pkg.toggle_action();
      return 1;
    }
  return 0;
}

ActionList *
PickPackageLine::get_actions(int col_num) const
{
  if (col_num == new_col)
    {
      return pkg.list_actions (theView.deftrust);
    }

  return NULL;
}

int
PickPackageLine::get_indent() const
{
  return indent;
}

int
PickPackageLine::map_key_to_action(WORD vkey, int modkeys, int & col_num,
                                   int & action_id) const
{
  switch (vkey)
    {
    case VK_SPACE: // install/reinstall/uninstall context menu for package
    case VK_APPS:
      col_num = new_col;
      return Action::PopUp;
    case 'I': // Ctrl+I: select install default version and move to next row
    case 'K': // Ctrl+K: select keep or skip package and move to next row
    case 'R': // Ctrl+R: select reinstall and move to next row
    case 'U': // Ctrl+U: select uninstall and move to next row
      if (modkeys != ModifierKeys::Control)
        break;
      col_num = new_col;
      switch (vkey)
        {
        case 'I': action_id = packagemeta::Install_action; break;
        default:  action_id = packagemeta::NoChange_action; break;
        case 'R': action_id = packagemeta::Reinstall_action; break;
        case 'U': action_id = packagemeta::Uninstall_action; break;
        }
      return Action::Direct | Action::NextRow;
    }

  return Action::None;
}
