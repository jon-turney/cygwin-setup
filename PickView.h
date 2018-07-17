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

#ifndef SETUP_PICKVIEW_H
#define SETUP_PICKVIEW_H

#include <string>

#include "package_meta.h"
#include "ListView.h"

class Window;
class CategoryTree;

class PickView
{
public:
  trusts deftrust; // XXX: needs accessor

  enum class views
  {
    PackageFull = 0,
    PackagePending,
    PackageKeeps,
    PackageSkips,
    PackageUserPicked,
    Category,
  };

  PickView ();
  ~PickView();
  void defaultTrust (trusts trust);
  void setViewMode (views mode);
  views getViewMode ();
  void init(views _mode, ListView *_listview, Window *parent);
  void build_category_tree();
  static const char *mode_caption (views mode);
  void setObsolete (bool doit);
  void refresh();
  void init_headers ();

  void SetPackageFilter (const std::string &filterString)
  {
    packageFilterString = filterString;
  }

  Window *GetParent(void) { return parent; }

private:
  views view_mode;
  ListView *listview;
  bool showObsolete;
  std::string packageFilterString;
  ListViewContents contents;
  CategoryTree *cat_tree_root;
  Window *parent;

  void insert_pkg (packagemeta &, int indent = 0);
  void insert_category (CategoryTree *);
};

enum
{
 pkgname_col = 0, // package/category name
 current_col = 1,
 new_col = 2,     // action
 bintick_col = 3,
 srctick_col = 4,
 cat_col = 5,
 size_col = 6,
 pkg_col = 7,     // desc
};

bool isObsolete (std::set <std::string, casecompare_lt_op> &categories);
bool isObsolete (const std::string& catname);

//
// Helper class which stores the contents and collapsed/expanded state for each
// category (and the pseudo-category 'All')
//

class CategoryTree
{
public:
  CategoryTree(Category & cat, bool collapsed) :
    _cat (cat),
    _collapsed(collapsed),
    _action (packagemeta::Default_action)
  {
  }

  ~CategoryTree()
  {
  }

  std::vector <CategoryTree *> & bucket()
  {
    return _bucket;
  }

  bool &collapsed()
  {
    return _collapsed;
  }

  const Category &category()
  {
    return _cat;
  }

  packagemeta::_actions & action()
  {
    return _action;
  }

  int do_action(packagemeta::_actions action_id, trusts const deftrust)
  {
    int u = 1;
    _action = action_id;

    if (_bucket.size())
      {
        for (std::vector <CategoryTree *>::const_iterator i = _bucket.begin();
             i != _bucket.end();
             i++)
          {
            // recurse for all contained categories
            int l = (*i)->do_action(action_id, deftrust);

            if (!_collapsed)
              u += l;
          }
      }
    else
      {
        // otherwise, this is a leaf category, so apply action to all packages
        // in this category
        int l = 0;
        for (std::vector <packagemeta *>::const_iterator pkg = _cat.second.begin();
             pkg != _cat.second.end();
             ++pkg)
          {
            (*pkg)->select_action(action_id, deftrust);
            l++;
          }

        // these lines need to be updated, if displayed
        if (!_collapsed)
          u += l;
      }
    return u;
  }

private:
  Category & _cat;
  bool _collapsed;
  std::vector <CategoryTree *> _bucket;
  packagemeta::_actions _action;
};

#endif /* SETUP_PICKVIEW_H */
