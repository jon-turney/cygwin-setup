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

#include "win32.h"
#include "String++.h"
#include "window.h"

#define HMARGIN 10
#define ROW_MARGIN      5
#define ICON_MARGIN     4
#define RTARROW_WIDTH 11
#define SPIN_WIDTH 11
#define NEW_COL_SIZE_SLOP (ICON_MARGIN + SPIN_WIDTH + RTARROW_WIDTH)
#define CHECK_SIZE      11

#define CATEGORY_EXPANDED  0
#define CATEGORY_COLLAPSED 1

class PickView;
#include "PickCategoryLine.h"
#include "package_meta.h"

class PickView : public Window
{
public:
  virtual bool Create (Window * Parent = NULL, DWORD Style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN, RECT * r = NULL);
  virtual bool registerWindowClass ();
  class views;
  class Header;
  int num_columns;
  views get_view_mode ()
  {
    return view_mode;
  };
  void defaultTrust (trusts trust);
  void set_view_mode (views _mode);
  void setViewMode (PickView::views mode);
  void paint (HWND hwnd);
  LRESULT CALLBACK list_click (HWND hwnd, BOOL dblclk, int x, int y, UINT hitCode);
  LRESULT CALLBACK list_hscroll (HWND hwnd, HWND hctl, UINT code, int pos);
  LRESULT CALLBACK list_vscroll (HWND hwnd, HWND hctl, UINT code, int pos);
  virtual LRESULT WindowProc (UINT uMsg, WPARAM wParam, LPARAM lParam);
  Header *headers;
  PickView (Category & cat);
  void init(views _mode);
  ~PickView();
  const char *mode_caption ();
  void insert_pkg (packagemeta &);
  void insert_category (Category *, bool);
  void clear_view (void);
  int click (int row, int x);
  void refresh();
  int current_col;
  int new_col;
  int bintick_col;
  int srctick_col;
  int cat_col;
  int pkg_col;
  int last_col;
  int row_height;
  TEXTMETRIC tm;
  HDC bitmap_dc;
  HANDLE bm_spin,bm_rtarrow, bm_checkyes, bm_checkno, bm_checkna;
  trusts deftrust;
  HANDLE sysfont;
  int scroll_ulc_x, scroll_ulc_y;
  int header_height;
  PickCategoryLine contents;
  void scroll (HWND hwnd, int which, int *var, int code);
  HWND ListHeader (void) const
  {
    return listheader;
  }

  class views
  {
  public:
    static const views Unknown;
    static const views PackageFull;
    static const views Package;
    static const views PackageKeeps;
    static const views PackageSkips;
    static const views Category;
    static const views NView;
      views ():_value (0)
    {
    };
    views (int aInt)
    {
      _value = aInt;
      if (_value < 0 || _value > 5)
	_value = 0;
    }
    views & operator++ ();
    bool operator == (views const &rhs)
    {
      return _value == rhs._value;
    }
    bool operator != (views const &rhs)
    {
      return _value != rhs._value;
    }
    const char *caption ();

  private:
    int _value;
  };

  class Header
  {
  public:
    const char *text;
    int slen;
    int width;
    int x;
  };

private:
  static ATOM WindowClassAtom;
  HWND listheader;
  views view_mode;
  void set_headers ();
  void init_headers (HDC dc);
  void note_width (Header *hdrs, HDC dc, String const &string, int addend,
      int column);
};

#endif /* SETUP_PICKVIEW_H */
