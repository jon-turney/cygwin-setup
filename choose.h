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

#include "ini.h"

#define CATEGORY_EXPANDED  0
#define CATEGORY_COLLAPSED 1


struct _header
{
  const char *text;
  int slen;
  int width;
  int x;
};

typedef class _view view;

class pick_line
{
  public:
    void set_line (Package *_pkg);
    void set_line (Category *_cat);
    void paint (HDC hdc, int x, int y, int row, int show_cat);
    Package *get_pkg (void) { return pkg; };
    Category *get_category (void) { return cat; };
    int click (int x);
  private:
    Package *pkg;
    Category *cat;
};

class _view
{
  public:
    int num_columns;
    views get_view_mode () { return view_mode; };
    void set_view_mode(views _mode);
    struct _header *headers;
    _view (views mode, HDC dc);
    const char *mode_caption ();
    void insert_pkg (Package *);
    void insert_category (Category *, int);
    void clear_view (void);
    int click (int row, int x);
    int current_col;
    int new_col;
    int src_col;
    int cat_col;
    int pkg_col;
    int last_col;
    pick_line * lines;
    int nlines;

  private:
    views view_mode;
    void set_headers (void);
    void init_headers (HDC dc);
    void insert_at (int, pick_line);
    void insert_under (int linen, pick_line line); 

};

#endif /* _CHOOSE_H_ */
