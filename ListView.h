/*
 * Copyright (c) 2016 Jon Turney
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#ifndef SETUP_LISTVIEW_H
#define SETUP_LISTVIEW_H

#include "ActionList.h"
#include "win32.h"
#include <commctrl.h>
#include <vector>

// ---------------------------------------------------------------------------
// interface to class ListView
//
// ListView Common Control
// ---------------------------------------------------------------------------

struct ModifierKeys
{
  enum { Shift = 0x01, Control = 0x02, Alt = 0x04 };
  static int get(); // get bitmask of currently pressed keys
};

class ListViewLine
{
 public:
  enum class State { collapsed, expanded, nothing=-1 };
  enum Action { None = 0x00, Direct = 0x01, PopUp = 0x02, NextRow = 0x04 };

  virtual ~ListViewLine() {};
  virtual const std::wstring get_text(int col) const = 0;
  virtual State get_state() const = 0;
  virtual const std::string get_tooltip(int col) const = 0;
  virtual int get_indent() const = 0;
  virtual ActionList *get_actions(int col) const = 0;
  virtual int do_action(int col, int id) = 0;
  virtual int do_default_action(int col) = 0;
  virtual int map_key_to_action(WORD vkey, int modkeys, int & col_num,
                                int & action_id) const = 0;
};

typedef std::vector<ListViewLine *> ListViewContents;

class ListView
{
 public:
  enum class ControlType
  {
    text,
    checkbox,
    popup,
  };

  class Header
  {
  public:
    unsigned int text; // resource id of header text
    int fmt;
    ControlType type;
    int width;
    int hdr_width;
  };
  typedef Header *HeaderList;

  void init(HWND parent, int id, HeaderList headers);

  void noteColumnWidthStart();
  template <typename T>
  void noteColumnWidth(int col_num, const T& string);
  void noteColumnWidthEnd();
  void resizeColumns(void);

  void setContents(ListViewContents *contents, bool tree = false);
  void setEmptyText(unsigned int text);

  bool OnNotify (NMHDR *pNmHdr, LRESULT *pResult);

 private:
  HWND hWndParent;
  HWND hWndListView;
  HWND hWndTip;
  HDC dc;
  HIMAGELIST hImgList;
  HIMAGELIST hEmptyImgList;

  ListViewContents *contents;
  HeaderList headers;
  std::wstring empty_list_text;
  int iRow_track;
  int iCol_track;

  void initColumns(HeaderList hl);
  void empty(void);
  int popup_menu(int iRow, int iCol, POINT p);
};

#endif /* SETUP_LISTVIEW_H */
