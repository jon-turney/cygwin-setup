/*
 * Copyright (c) 2000, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <rbtcollins@hotmail.com>
 *
 */

#ifndef _CHOOSE_H_
#define _CHOOSE_H_

#include "proppage.h"

class Category;
class packagemeta;

#define CATEGORY_EXPANDED  0
#define CATEGORY_COLLAPSED 1

typedef enum
{
  /* Note that the next four items must be in the same order as the
     TRUST items in ini.h. */
  ACTION_UNKNOWN,
  ACTION_PREV,
  ACTION_CURR,
  ACTION_TEST,
  ACTION_SKIP,
  ACTION_UNINSTALL,
  ACTION_REDO,
  ACTION_SRC_ONLY,
  ACTION_LAST,
  ACTION_ERROR,
  /* Use ACTION_SAME when you want to leve the current version unaltered
   * even if it that version is not in setup.ini
   */
  ACTION_SAME = 100,
  /* Actions taken when installed version matches the selected version. */
  ACTION_SAME_PREV = ACTION_PREV + ACTION_SAME,
  ACTION_SAME_CURR = ACTION_CURR + ACTION_SAME,
  ACTION_SAME_TEST = ACTION_TEST + ACTION_SAME,
  /* Last action. */
  ACTION_SAME_LAST
}
actions;

typedef enum
{
  VIEW_UNKNOWN,
  VIEW_PACKAGE_FULL,
  VIEW_PACKAGE,
  VIEW_CATEGORY,
  NVIEW
}
views;

struct _header
{
  const char *text;
  int slen;
  int width;
  int x;
};

#ifdef __cplusplus
#include "list.h"
#include "package_meta.h"

class view;

class pick_line
{
public:
  virtual void paint (HDC hdc, int x, int y, int row, int show_cat) = 0;
  virtual int click (int const myrow, int const ClickedRow, int const x) = 0;
  virtual int itemcount () const = 0;
  // this may indicate bad inheritance model.
  virtual bool IsContainer (void) const = 0;
  virtual void insert (pick_line &) = 0;
  // Never allocate to key, always allocated elsewhere
  char const *key;
    virtual ~ pick_line ()
  {
  };
protected:
  pick_line ()
  {
  };
  pick_line (pick_line const &);
  pick_line & operator= (pick_line const &);
};

class pick_pkg_line:public pick_line
{
public:
  pick_pkg_line (packagemeta & apkg):pkg (apkg)
  {
    key = apkg.key;
  };
  virtual void paint (HDC hdc, int x, int y, int row, int show_cat);
  virtual int click (int const myrow, int const ClickedRow, int const x);
  virtual int itemcount () const
  {
    return 1;
  }
  virtual bool IsContainer (void) const
  {
    return false;
  }
  virtual void insert (pick_line &)
  {
  };
private:
  packagemeta & pkg;
};

class topbucket:public pick_line
{
public:
  topbucket ()
  {
    key = 0;
  };
  virtual void paint (HDC hdc, int x, int y, int row, int show_cat);
  virtual int click (int const myrow, int const ClickedRow, int const x);
  virtual int itemcount () const
  {
    int t = 0;
    for (size_t n = 1; n <= bucket.number (); n++)
        t += bucket[n]->itemcount ();
      return t;
  };
  virtual bool IsContainer (void) const
  {
    return true;
  }
  virtual void insert (pick_line & aLine)
  {
    bucket.registerbyobject (aLine);
  }
  virtual void empty (void);
  virtual ~ topbucket ();
protected:
  topbucket (topbucket const &);
  topbucket & operator= (topbucket const &);
private:
  list < pick_line, char const *, strcasecmp > bucket;
};


class pick_category_line:public topbucket
{
public:
  pick_category_line (Category & _cat, bool aBool =
		      true):cat (_cat), collapsed (aBool)
  {
    key = _cat.key;
  };
  virtual void paint (HDC hdc, int x, int y, int row, int show_cat);
  virtual int click (int const myrow, int const ClickedRow, int const x);
  virtual int itemcount () const
  {
    if (collapsed)
      return 1;
    int t = 1;
    for (size_t n = 1; n <= bucket.number (); n++)
        t += bucket[n]->itemcount ();
      return t;
  };
  virtual void insert (pick_line & aLine)
  {
    bucket.registerbyobject (aLine);
  }
private:
  Category & cat;
  list < pick_line, char const *, strcasecmp > bucket;
  bool collapsed;
};

class view
{
public:
  int num_columns;
  views get_view_mode ()
  {
    return view_mode;
  };
  void set_view_mode (views _mode);
  struct _header *headers;
  view (views mode, HWND listview);
  const char *mode_caption ();
  void insert_pkg (packagemeta &);
  void insert_category (Category *, bool);
  void clear_view (void);
  int click (int row, int x);
  int current_col;
  int new_col;
  int src_col;
  int cat_col;
  int pkg_col;
  int last_col;
//  pick_line *lines;
//  int nlines;
  topbucket contents;
  void scroll (HWND hwnd, int which, int *var, int code);
  HWND ListHeader (void) const
  {
    return listheader;
  }

private:
    HWND listview;
  HWND listheader;
  views view_mode;
  void set_headers ();
  void init_headers (HDC dc);

};

class ChooserPage:public PropertyPage
{
public:
  ChooserPage ()
  {
  };
  virtual ~ ChooserPage ()
  {
  };

  virtual bool OnMessageApp (UINT uMsg, WPARAM wParam, LPARAM lParam);

  bool Create ();
  virtual void OnActivate ();
};

#endif /* __cplusplus */
#endif /* _CHOOSE_H_ */
