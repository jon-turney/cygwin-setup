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

typedef class _view view;

class pick_line
{
public:
  void set_line (packagemeta * _pkg);
  void set_line (Category * _cat);
  void paint (HDC hdc, int x, int y, int row, int show_cat);
  packagemeta *get_pkg (void)
  {
    return pkg;
  };
  Category *get_category (void)
  {
    return cat;
  };
  int click (int x);
private:
  packagemeta * pkg;
  Category *cat;
};

class _view
{
public:
  int num_columns;
  views get_view_mode ()
  {
    return view_mode;
  };
  void set_view_mode (views _mode);
  struct _header *headers;
  _view (views mode, HDC dc);
  const char *mode_caption ();
  void insert_pkg (packagemeta &);
  void insert_category (Category *, int);
  void clear_view (void);
  int click (int row, int x);
  int current_col;
  int new_col;
  int src_col;
  int cat_col;
  int pkg_col;
  int last_col;
  pick_line *lines;
  int nlines;

private:
  views view_mode;
  void set_headers (void);
  void init_headers (HDC dc);
  void insert_at (int, pick_line);
  void insert_under (int linen, pick_line line);

};
#endif /* __cplusplus */
#endif /* _CHOOSE_H_ */
