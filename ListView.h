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

#include "win32.h"
#include <vector>

// ---------------------------------------------------------------------------
// interface to class ListView
//
// ListView Common Control
// ---------------------------------------------------------------------------

class ListViewLine
{
 public:
  virtual ~ListViewLine() {};
  virtual const std::string get_text(int col) const = 0;
  virtual int do_action(int col) = 0;
};

typedef std::vector<ListViewLine *> ListViewContents;

class ListView
{
 public:
  class Header
  {
  public:
    const char *text;
    int fmt;
    int width;
    int hdr_width;
  };
  typedef Header *HeaderList;

  void init(HWND parent, int id, HeaderList headers);

  void noteColumnWidthStart();
  void noteColumnWidth(int col_num, const std::string& string);
  void noteColumnWidthEnd();
  void resizeColumns(void);

  void setContents(ListViewContents *contents);
  void setEmptyText(const char *text);

  bool OnNotify (NMHDR *pNmHdr, LRESULT *pResult);

 private:
  HWND hWndParent;
  HWND hWndListView;
  HDC dc;
  ListViewContents *contents;
  HeaderList headers;
  const char *empty_list_text;

  void initColumns(HeaderList hl);
  void empty(void);
};

#endif /* SETUP_LISTVIEW_H */
