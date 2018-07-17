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

class ListViewLine
{
 public:
  enum class State { collapsed, expanded, nothing=-1 };

  virtual ~ListViewLine() {};
  virtual const std::string get_text(int col) const = 0;
  virtual State get_state() const = 0;
  virtual int get_indent() const = 0;
  virtual ActionList *get_actions(int col) const = 0;
  virtual int do_action(int col, int id) = 0;
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
    const char *text;
    int fmt;
    ControlType type;
    int width;
    int hdr_width;
  };
  typedef Header *HeaderList;

  void init(HWND parent, int id, HeaderList headers);

  void noteColumnWidthStart();
  void noteColumnWidth(int col_num, const std::string& string);
  void noteColumnWidthEnd();
  void resizeColumns(void);

  void setContents(ListViewContents *contents, bool tree = false);
  void setEmptyText(const char *text);

  bool OnNotify (NMHDR *pNmHdr, LRESULT *pResult);

 private:
  HWND hWndParent;
  HWND hWndListView;
  HDC dc;
  HIMAGELIST hImgList;
  HIMAGELIST hEmptyImgList;

  ListViewContents *contents;
  HeaderList headers;
  const char *empty_list_text;

  void initColumns(HeaderList hl);
  void empty(void);
  int popup_menu(int iRow, int iCol, POINT p);
};

#endif /* SETUP_LISTVIEW_H */
