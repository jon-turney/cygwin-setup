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

#include "ListView.h"
#include "LogSingleton.h"

#include <commctrl.h>

// ---------------------------------------------------------------------------
// implements class ListView
//
// ListView Common Control
// ---------------------------------------------------------------------------

void
ListView::init(HWND parent, int id, HeaderList headers)
{
  hWndParent = parent;

  // locate the listview control
  hWndListView = ::GetDlgItem(parent, id);

  // configure the listview control
  SendMessage(hWndListView, CCM_SETVERSION, 6, 0);

  ListView_SetExtendedListViewStyle(hWndListView,
                                    LVS_EX_COLUMNSNAPPOINTS | // use cxMin
                                    LVS_EX_FULLROWSELECT |
                                    LVS_EX_GRIDLINES |
                                    LVS_EX_HEADERDRAGDROP);   // headers can be re-ordered

  // give the header control a border
  HWND hWndHeader = ListView_GetHeader(hWndListView);
  SetWindowLongPtr(hWndHeader, GWL_STYLE,
                   GetWindowLongPtr(hWndHeader, GWL_STYLE) | WS_BORDER);

  // ensure an initial item exists for width calculations...
  LVITEM lvi;
  lvi.mask = LVIF_TEXT;
  lvi.iItem = 0;
  lvi.iSubItem = 0;
  lvi.pszText = const_cast <char *> ("Working...");
  ListView_InsertItem(hWndListView, &lvi);

  // populate with columns
  initColumns(headers);
}

void
ListView::initColumns(HeaderList headers_)
{
  // store HeaderList for later use
  headers = headers_;

  // create the columns
  LVCOLUMN lvc;
  lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

  int i;
  for (i = 0; headers[i].text != 0; i++)
    {
      lvc.iSubItem = i;
      lvc.pszText = const_cast <char *> (headers[i].text);
      lvc.cx = 100;
      lvc.fmt = headers[i].fmt;

      ListView_InsertColumn(hWndListView, i, &lvc);
    }

  // now do some width calculations
  for (i = 0; headers[i].text != 0; i++)
    {
      headers[i].width = 0;

      ListView_SetColumnWidth(hWndListView, i, LVSCW_AUTOSIZE_USEHEADER);
      headers[i].hdr_width = ListView_GetColumnWidth(hWndListView, i);
    }
}

void
ListView::noteColumnWidthStart()
{
  dc = GetDC (hWndListView);

  // we must set the font of the DC here, otherwise the width calculations
  // will be off because the system will use the wrong font metrics
  HANDLE sysfont = GetStockObject (DEFAULT_GUI_FONT);
  SelectObject (dc, sysfont);

  int i;
  for (i = 0; headers[i].text != 0; i++)
    {
      headers[i].width = 0;
    }
}

void
ListView::noteColumnWidth(int col_num, const std::string& string)
{
  SIZE s = { 0, 0 };

  // A margin of 3*GetSystemMetrics(SM_CXEDGE) is used at each side of the
  // header text.
  int addend = 2*3*GetSystemMetrics(SM_CXEDGE);

  if (string.size())
    GetTextExtentPoint32 (dc, string.c_str(), string.size(), &s);

  int width = addend + s.cx;

  if (width > headers[col_num].width)
    headers[col_num].width = width;
}

void
ListView::noteColumnWidthEnd()
{
  ReleaseDC(hWndListView, dc);
}

void
ListView::resizeColumns(void)
{
  // ensure the last column stretches all the way to the right-hand side of the
  // listview control
  int i;
  int total = 0;
  for (i = 0; headers[i].text != 0; i++)
    total = total + headers[i].width;

  RECT r;
  GetClientRect(hWndListView, &r);
  int width = r.right - r.left;

  if (total < width)
    headers[i-1].width += width - total;

  // size each column
  LVCOLUMN lvc;
  lvc.mask = LVCF_WIDTH | LVCF_MINWIDTH;
  for (i = 0; headers[i].text != 0; i++)
    {
      lvc.iSubItem = i;
      lvc.cx = (headers[i].width < headers[i].hdr_width) ? headers[i].hdr_width : headers[i].width;
      lvc.cxMin = headers[i].hdr_width;
#if DEBUG
      Log (LOG_BABBLE) << "resizeColumns: " << i << " cx " << lvc.cx << " cxMin " << lvc.cxMin <<endLog;
#endif

      ListView_SetColumn(hWndListView, i, &lvc);
    }
}

void
ListView::setContents(ListViewContents *_contents)
{
  contents = _contents;

  // disable redrawing of ListView
  // (otherwise it will redraw every time a row is added, which makes this very slow)
  SendMessage(hWndListView, WM_SETREDRAW, FALSE, 0);

  // preserve focus/selection
  int iRow = ListView_GetSelectionMark(hWndListView);

  empty();

  size_t i;
  for (i = 0; i < contents->size();  i++)
    {
      LVITEM lvi;
      lvi.mask = LVIF_TEXT;
      lvi.iItem = i;
      lvi.iSubItem = 0;
      lvi.pszText = LPSTR_TEXTCALLBACK;

      ListView_InsertItem(hWndListView, &lvi);
    }

  if (iRow >= 0)
    {
      ListView_SetItemState(hWndListView, iRow, LVNI_SELECTED | LVNI_FOCUSED, LVNI_SELECTED | LVNI_FOCUSED);
      ListView_EnsureVisible(hWndListView, iRow, false);
    }

  // enable redrawing of ListView and redraw
  SendMessage(hWndListView, WM_SETREDRAW, TRUE, 0);
  RedrawWindow(hWndListView, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Helper class: The char * pointer we hand back needs to remain valid for some
// time after OnNotify returns, when the std::string we have retrieved has gone
// out of scope, so a static instance of this class maintains a local cache.
class StringCache
{
public:
  StringCache() : cache(NULL), cache_size(0) { }
  StringCache & operator = (const std::string & s)
  {
    if ((s.length() + 1) > cache_size)
      {
        cache_size = s.length() + 1;
        cache = (char *)realloc(cache, cache_size);
      }
    strcpy(cache, s.c_str());
    return *this;
  }
  operator char *() const
  {
    return cache;
  }
private:
  char *cache;
  size_t cache_size;
};

bool
ListView::OnNotify (NMHDR *pNmHdr, LRESULT *pResult)
{
#if DEBUG
  Log (LOG_BABBLE) << "ListView::OnNotify id:" << pNmHdr->idFrom << " hwnd:" << pNmHdr->hwndFrom << " code:" << (int)pNmHdr->code << endLog;
#endif

  switch (pNmHdr->code)
  {
  case LVN_GETDISPINFO:
    {
      NMLVDISPINFO *pNmLvDispInfo = (NMLVDISPINFO *)pNmHdr;
#if DEBUG
      Log (LOG_BABBLE) << "LVN_GETDISPINFO " << pNmLvDispInfo->item.iItem << endLog;
#endif
      if (contents)
        {
          int iRow = pNmLvDispInfo->item.iItem;
          int iCol = pNmLvDispInfo->item.iSubItem;

          static StringCache s;
          s = (*contents)[iRow]->get_text(iCol);
          pNmLvDispInfo->item.pszText = s;
        }

      return true;
    }
    break;

  case LVN_GETEMPTYMARKUP:
    {
      NMLVEMPTYMARKUP *pNmMarkup = (NMLVEMPTYMARKUP*) pNmHdr;

      MultiByteToWideChar(CP_UTF8, 0,
                          empty_list_text, -1,
                          pNmMarkup->szMarkup, L_MAX_URL_LENGTH);

      *pResult = true;
      return true;
    }
    break;

  case NM_CLICK:
    {
      NMITEMACTIVATE *pNmItemAct = (NMITEMACTIVATE *) pNmHdr;
#if DEBUG
      Log (LOG_BABBLE) << "NM_CLICK: pnmitem->iItem " << pNmItemAct->iItem << " pNmItemAct->iSubItem " << pNmItemAct->iSubItem << endLog;
#endif
      int iRow = pNmItemAct->iItem;
      int iCol = pNmItemAct->iSubItem;

      if (iRow >= 0)
        {
          // Inform the item of the click
          int update = (*contents)[iRow]->do_action(iCol);

          // Update items, if needed
          if (update > 0)
            {
              ListView_RedrawItems(hWndListView, iRow, iRow + update -1);
            }
        }
      return true;
    }
    break;
  }

  // We don't care.
  return false;
}

void
ListView::empty(void)
{
  ListView_DeleteAllItems(hWndListView);
}

void
ListView::setEmptyText(const char *text)
{
  empty_list_text = text;
}
