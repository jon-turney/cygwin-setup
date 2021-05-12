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

#include "PickView.h"
#include "PickPackageLine.h"
#include "PickCategoryLine.h"
#include <algorithm>
#include <limits.h>
#include <shlwapi.h>

#include "package_db.h"
#include "dialog.h"
#include "resource.h"
/* For 'source' */
#include "state.h"
#include "LogSingleton.h"
#include "Exception.h"

void
PickView::setViewMode (views mode)
{
  view_mode = mode;
  packagedb db;

  size_t i;
  for (i = 0; i < contents.size();  i++)
    {
      delete contents[i];
    }
  contents.clear();

  if (view_mode == PickView::views::Category)
    {
      insert_category (cat_tree_root);
    }
  else
    {
      // iterate through every package
      for (packagedb::packagecollection::iterator i = db.packages.begin ();
            i != db.packages.end (); ++i)
        {
          packagemeta & pkg = *(i->second);

          if (!pkg.isBinary())
            continue;

          if ( // "Full" : everything
              (view_mode == PickView::views::PackageFull)

              // "Pending" : packages that are being added/removed/upgraded
              || (view_mode == PickView::views::PackagePending &&
                  ((!pkg.desired && pkg.installed) ||         // uninstall
                    (pkg.desired &&
                      (pkg.picked () ||               // install bin
                       pkg.srcpicked ())))) // src

              // "Up to date" : installed packages that will not be changed
              || (view_mode == PickView::views::PackageKeeps &&
                  (pkg.installed && pkg.desired && !pkg.picked ()
                    && !pkg.srcpicked ()))

              // "Not installed"
              || (view_mode == PickView::views::PackageSkips &&
                  (!pkg.desired && !pkg.installed))

              // "UserPick" : installed packages that were picked by user
              || (view_mode == PickView::views::PackageUserPicked &&
                  (pkg.installed && pkg.user_picked)))
            {
              // Filter by package name
              if (packageFilterString.empty ()
                  || StrStrI (pkg.name.c_str (), packageFilterString.c_str ()))
                insert_pkg (pkg);
            }
        }
    }

  listview->setContents(&contents, view_mode == PickView::views::Category);
}

PickView::views
PickView::getViewMode ()
{
  return view_mode;
}

const char *
PickView::mode_caption (views mode)
{
  switch (mode)
    {
    case views::PackageFull:
      return "Full";
    case views::PackagePending:
      return "Pending";
    case views::PackageKeeps:
      return "Up To Date";
    case views::PackageSkips:
      return "Not Installed";
    case views::PackageUserPicked:
      return "Picked";
    case views::Category:
      return "Category";
    default:
      return "";
    }
}

/* meant to be called on packagemeta::categories */
bool
isObsolete (std::set <std::string, casecompare_lt_op> &categories)
{
  std::set <std::string, casecompare_lt_op>::const_iterator i;
  
  for (i = categories.begin (); i != categories.end (); ++i)
    if (isObsolete (*i))
      return true;
  return false;
}

bool
isObsolete (const std::string& catname)
{
  if (casecompare(catname, "ZZZRemovedPackages") == 0
        || casecompare(catname, "_", 1) == 0)
    return true;
  return false;
}

/* Sets the mode for showing/hiding obsolete junk packages.  */
void
PickView::setObsolete (bool doit)
{
  showObsolete = doit;
  refresh ();
}

void
PickView::insert_pkg (packagemeta & pkg, int indent)
{
  if (!showObsolete && isObsolete (pkg.categories))
    return;

  contents.push_back(new PickPackageLine(*this, pkg, indent));
}

void
PickView::insert_category (CategoryTree *cat_tree)
{
  if (!cat_tree)
    return;

  const Category *cat = &(cat_tree->category());

  // Suppress obsolete category when not showing obsolete
  if ((!showObsolete && isObsolete (cat->first)))
    return;

  // if it's not the "All" category
  int packageCount = 0;
  bool hasContents = false;
  bool isAll = casecompare(cat->first, "All") == 0;
  if (!isAll)
    {
      // count the number of packages in this category
      for (std::vector <packagemeta *>::const_iterator i = cat->second.begin ();
           i != cat->second.end () ; ++i)
        {
          if (packageFilterString.empty ()      \
              || (*i
                  && StrStrI ((*i)->name.c_str (), packageFilterString.c_str ())))
            {
              packageCount++;
            }
        }

      // if there are some packages in the category, or we are showing everything,
      if (packageFilterString.empty () || packageCount)
        {
          hasContents = true;
        }
    }

  if (!isAll && !hasContents)
    return;

  // insert line for the category
  contents.push_back(new PickCategoryLine(*this, cat_tree, packageCount, isAll ? 0 : 1));

  // if not collapsed
  if (!cat_tree->collapsed())
    {
      // insert lines for the packages in this category
      if (!isAll)
        {
          for (std::vector <packagemeta *>::const_iterator i = cat->second.begin ();
               i != cat->second.end () ; ++i)
            {
              if (packageFilterString.empty ()  \
                  || (*i
                      && StrStrI ((*i)->name.c_str (), packageFilterString.c_str ())))
                {
                  insert_pkg(**i, 2);
                }
            }
        }

      // recurse for contained categories
      for (std::vector <CategoryTree *>::iterator i = cat_tree->bucket().begin ();
           i != cat_tree->bucket().end();
           i++)
        {
          insert_category(*i);
        }
    }
}

/* this means to make the 'category' column wide enough to fit the first 'n'
   categories for each package.  */
#define NUM_CATEGORY_COL_WIDTH 2

void
PickView::init_headers (void)
{
  listview->noteColumnWidthStart();

  // width of the 'src' checkbox column just needs to accommodate the
  // column name
  listview->noteColumnWidth (srctick_col, "");

  // (In category view) accommodate the width of each category name
  packagedb db;
  for (packagedb::categoriesType::iterator n = packagedb::categories.begin();
       n != packagedb::categories.end(); ++n)
    {
      listview->noteColumnWidth (cat_col, n->first);
    }

  /* For each package, accomodate the width of the installed version in the
     current_col, the widths of all other versions in the new_col, and the width
     of the sdesc for the pkg_col and the first NUM_CATEGORY_COL_WIDTH
     categories in the category column. */
  for (packagedb::packagecollection::iterator n = db.packages.begin ();
       n != db.packages.end (); ++n)
    {
      packagemeta & pkg = *(n->second);
      if (pkg.installed)
        listview->noteColumnWidth (current_col, pkg.installed.Canonical_version ());
      for (std::set<packageversion>::iterator i = pkg.versions.begin ();
           i != pkg.versions.end (); ++i)
        {
          if (*i != pkg.installed)
            listview->noteColumnWidth (new_col, i->Canonical_version ());
          std::string z = format_1000s(i->source ()->size);
          listview->noteColumnWidth (size_col, z);
          z = format_1000s(i->sourcePackage ().source ()->size);
          listview->noteColumnWidth (size_col, z);
        }
      std::string s = pkg.name;
      listview->noteColumnWidth (pkgname_col, s);

      s = pkg.SDesc ();
      listview->noteColumnWidth (pkg_col, s);

      if (pkg.categories.size () > 2)
        {
          std::string compound_cat("");
          std::set<std::string, casecompare_lt_op>::const_iterator cat;
          size_t cnt;

          for (cnt = 0, cat = pkg.categories.begin ();
               cnt < NUM_CATEGORY_COL_WIDTH && cat != pkg.categories.end ();
               ++cat)
            {
              if (casecompare(*cat, "All") == 0)
                continue;
              if (compound_cat.size ())
                compound_cat += ", ";
              compound_cat += *cat;
              cnt++;
            }
          listview->noteColumnWidth (cat_col, compound_cat);
        }
    }

  // ensure that the new_col is wide enough for all the labels
  const char *captions[] = { "Uninstall", "Skip", "Reinstall", "Retrieve",
                             "Source", "Keep", NULL };
  for (int i = 0; captions[i]; i++)
    listview->noteColumnWidth (new_col, captions[i]);

  listview->noteColumnWidthEnd();
  listview->resizeColumns();
}

PickView::PickView() :
  deftrust (TRUST_UNKNOWN),
  showObsolete (false),
  packageFilterString (),
  cat_tree_root (NULL)
{
}

void
PickView::init(views _mode, ListView *_listview, Window *_parent)
{
  view_mode = _mode;
  listview = _listview;
  parent = _parent;
}

void
PickView::build_category_tree()
{
  /* Build the category tree */

  /* Start collapsed. TODO: make that a flag */
  bool collapsed = true;

  /* Dispose of any existing category tree */
  if (cat_tree_root)
    {
      for (std::vector <CategoryTree *>::const_iterator i = cat_tree_root->bucket().begin();
           i != cat_tree_root->bucket().end();
           i++)
        delete *i;

      delete cat_tree_root;
      cat_tree_root = NULL;
    }

  /* Find the 'All' category */
  for (packagedb::categoriesType::iterator n =
         packagedb::categories.begin(); n != packagedb::categories.end();
       ++n)
    {
      if (casecompare(n->first, "All") == 0)
        {
          cat_tree_root = new CategoryTree(*n, collapsed);
          break;
        }
    }

  /* Add all the other categories as children */
  for (packagedb::categoriesType::iterator n =
         packagedb::categories.begin(); n != packagedb::categories.end();
       ++n)
    {
      if (casecompare(n->first, "All") == 0)
        continue;

      CategoryTree *cat_tree = new CategoryTree(*n, collapsed);
      cat_tree_root->bucket().push_back(cat_tree);
    }

  refresh ();
}

PickView::~PickView()
{
}

void
PickView::defaultTrust (trusts trust)
{
  this->deftrust = trust;
}

void
PickView::refresh()
{
  setViewMode (view_mode);
}
