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
#include "resource.h"

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

  // create a small icon imagelist
  // (the order of images matches ListViewLine::State enum)
  hImgList = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
                              GetSystemMetrics(SM_CYSMICON),
                              ILC_COLOR32, 2, 0);
  ImageList_AddIcon(hImgList, LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_TREE_PLUS)));
  ImageList_AddIcon(hImgList, LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_TREE_MINUS)));

  // create an empty imagelist, used to reset the indent
  hEmptyImgList = ImageList_Create(1, 1,
                                   ILC_COLOR32, 2, 0);

  // LVS_EX_INFOTIP/LVN_GETINFOTIP doesn't work for subitems, so we have to do
  // our own tooltip handling
  hWndTip = CreateWindowEx (0,
                            (LPCTSTR) TOOLTIPS_CLASS,
                            NULL,
                            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                            hWndParent,
                            (HMENU) 0,
                            GetModuleHandle(NULL),
                            NULL);
  // must be topmost so that tooltips will display on top
  SetWindowPos(hWndTip, HWND_TOPMOST,0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

  TOOLINFO ti;
  memset ((void *)&ti, 0, sizeof(ti));
  ti.cbSize = sizeof(ti);
  ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  ti.hwnd = hWndParent;
  ti.uId = (UINT_PTR)hWndListView;
  ti.lpszText =  LPSTR_TEXTCALLBACK; // use TTN_GETDISPINFO
  SendMessage(hWndTip, TTM_ADDTOOL, 0, (LPARAM)&ti);

  // match long delay for tooltip to disappear used elsewhere (30s)
  SendMessage(hWndTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, (LPARAM) MAKELONG (30000, 0));
  // match tip width used elsewhere
  SendMessage(hWndTip, TTM_SETMAXTIPWIDTH, 0, 450);
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
ListView::setContents(ListViewContents *_contents, bool tree)
{
  contents = _contents;

  // disable redrawing of ListView
  // (otherwise it will redraw every time a row is added, which makes this very slow)
  SendMessage(hWndListView, WM_SETREDRAW, FALSE, 0);

  // preserve focus/selection
  int iRow = ListView_GetSelectionMark(hWndListView);

  empty();

  // assign imagelist to listview control (this also sets the size for indents)
  if (tree)
    ListView_SetImageList(hWndListView, hImgList, LVSIL_SMALL);
  else
    ListView_SetImageList(hWndListView, hEmptyImgList, LVSIL_SMALL);

  size_t i;
  for (i = 0; i < contents->size();  i++)
    {
      LVITEM lvi;
      lvi.mask = LVIF_TEXT | (tree ? LVIF_IMAGE | LVIF_INDENT : 0);
      lvi.iItem = i;
      lvi.iSubItem = 0;
      lvi.pszText = LPSTR_TEXTCALLBACK;
      if (tree)
        {
          lvi.iImage = I_IMAGECALLBACK;
          lvi.iIndent = (*contents)[i]->get_indent();
        }

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

          if (pNmLvDispInfo->item.iSubItem == 0)
            {
              pNmLvDispInfo->item.iImage = (int)((*contents)[pNmLvDispInfo->item.iItem]->get_state());
            }
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
      if (iRow < 0)
        return false;

      int update = 0;

      if ((headers[iCol].type == ListView::ControlType::popup) ||
          (headers[iCol].type == ListView::ControlType::dropdown))
        {
          POINT p;
          if (headers[iCol].type == ListView::ControlType::dropdown)
            {
              // position drop-down menu over the subitem
              RECT r;
              ListView_GetSubItemRect(hWndListView, iRow, iCol, LVIR_BOUNDS, &r);
              p.x = r.left;
              p.y = r.top;
              ClientToScreen(hWndListView, &p);
            }
          else if (headers[iCol].type == ListView::ControlType::popup)
            {
              // position pop-up menu at the location of the click
              GetCursorPos(&p);
            }

          update = popup_menu(iRow, iCol, p);
        }
      else
        {
          // Inform the item of the click
          update = (*contents)[iRow]->do_action(iCol, 0);
        }

      // Update items, if needed
      if (update > 0)
        {
          ListView_RedrawItems(hWndListView, iRow, iRow + update -1);
        }

      return true;
    }
    break;

  case NM_CUSTOMDRAW:
    {
      NMLVCUSTOMDRAW *pNmLvCustomDraw = (NMLVCUSTOMDRAW *)pNmHdr;

      switch(pNmLvCustomDraw->nmcd.dwDrawStage)
        {
        case CDDS_PREPAINT:
          *pResult = CDRF_NOTIFYITEMDRAW;
          return true;
        case CDDS_ITEMPREPAINT:
          *pResult = CDRF_NOTIFYSUBITEMDRAW;
          return true;
        case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
          {
            LRESULT result = CDRF_DODEFAULT;
            int iCol = pNmLvCustomDraw->iSubItem;
            int iRow = pNmLvCustomDraw->nmcd.dwItemSpec;

            switch (headers[iCol].type)
              {
              default:
              case ListView::ControlType::text:
                result = CDRF_DODEFAULT;
                break;

              case ListView::ControlType::checkbox:
                {
                  // get the subitem text
                  char buf[3];
                  ListView_GetItemText(hWndListView, iRow, iCol, buf, _countof(buf));

                  // map the subitem text to a checkbox state
                  UINT state = DFCS_BUTTONCHECK | DFCS_FLAT;
                  if (buf[0] == '\0')                           // empty
                    {
                      result = CDRF_DODEFAULT;
                      break;
                    }
                  else if (buf[0] == 'y')                       // yes
                    state |= DFCS_CHECKED;
                  else if ((buf[0] == 'n') && (buf[1] == 'o'))  // no
                    state |= 0;
                  else                                          // n/a
                    state |= DFCS_INACTIVE;

                  // erase and draw a checkbox
                  RECT r;
                  ListView_GetSubItemRect(hWndListView, iRow, iCol, LVIR_BOUNDS, &r);
                  HBRUSH hBrush = CreateSolidBrush(ListView_GetBkColor(hWndListView));
                  FillRect(pNmLvCustomDraw->nmcd.hdc, &r, hBrush);
                  DeleteObject(hBrush);
                  DrawFrameControl(pNmLvCustomDraw->nmcd.hdc, &r, DFC_BUTTON, state);

                  result = CDRF_SKIPDEFAULT;
                }
                break;

              case ListView::ControlType::popup:
              case ListView::ControlType::dropdown:
                {
                  // let the control draw the text, but notify us afterwards
                  result = CDRF_NOTIFYPOSTPAINT;
                }
                break;
              }

            *pResult = result;
            return true;
          }
        case CDDS_SUBITEM | CDDS_ITEMPOSTPAINT:
          {
            LRESULT result = CDRF_DODEFAULT;
            int iCol = pNmLvCustomDraw->iSubItem;
            int iRow = pNmLvCustomDraw->nmcd.dwItemSpec;

            switch (headers[iCol].type)
              {
              default:
                result = CDRF_DODEFAULT;
                break;

              case ListView::ControlType::popup:
              case ListView::ControlType::dropdown:
                {
                  // draw the control at the RHS of the cell
                  RECT r;
                  ListView_GetSubItemRect(hWndListView, iRow, iCol, LVIR_BOUNDS, &r);
                  r.left = r.right - GetSystemMetrics(SM_CXVSCROLL);
                  DrawFrameControl(pNmLvCustomDraw->nmcd.hdc, &r, DFC_SCROLL,DFCS_SCROLLCOMBOBOX);

                  result = CDRF_DODEFAULT;
                }
                break;
              }
            *pResult = result;
            return true;
          }
        }
    }
    break;

  case LVN_HOTTRACK:
    {
      NMLISTVIEW *pNmListView = (NMLISTVIEW *)pNmHdr;
      int iRow = pNmListView->iItem;
      int iCol = pNmListView->iSubItem;
#if DEBUG
      Log (LOG_BABBLE) << "LVN_HOTTRACK " << iRow << " " << iCol << endLog;
#endif
      if (iRow < 0)
        return true;

      // if we've tracked off to a different cell
      if ((iRow != iRow_track) || (iCol != iCol_track))
        {
#if DEBUG
          Log (LOG_BABBLE) << "LVN_HOTTRACK changed cell" << endLog;
#endif

          // if the tooltip for previous cell is displayed, remove it
          // restart the tooltip AUTOPOP timer for this cell
          SendMessage(hWndTip, TTM_ACTIVATE, FALSE, 0);
          SendMessage(hWndTip, TTM_ACTIVATE, TRUE, 0);

          iRow_track = iRow;
          iCol_track = iCol;
        }

      return true;
    }
    break;

  case NM_RETURN:
    Log (LOG_PLAIN) << "NM_RETURN" << endLog;
    return true;
    break;

  case LVN_KEYDOWN:
    {
      NMLVKEYDOWN *pNmLvKeyDown = (NMLVKEYDOWN *)pNmHdr;
      int iRow = ListView_GetSelectionMark(hWndListView);
      Log (LOG_PLAIN) << "LVN_KEYDOWN vkey " << pNmLvKeyDown->wVKey << " on row " << iRow << endLog;

      if (contents && iRow >= 0)
        {
          int col_num;
          int action_id;
          if ((*contents)[iRow]->map_key_to_action(pNmLvKeyDown->wVKey, &col_num, &action_id))
            {
              int update;
              if (action_id >= 0)
                update = (*contents)[iRow]->do_action(col_num, action_id);
              else
                {
                  POINT p;
                  RECT r;
                  ListView_GetSubItemRect(hWndListView, iRow, col_num, LVIR_BOUNDS, &r);
                  p.x = r.left;
                  p.y = r.top;
                  ClientToScreen(hWndListView, &p);

                  update = popup_menu(iRow, col_num, p);
                }

              if (update > 0)
                ListView_RedrawItems(hWndListView, iRow, iRow + update -1);
            }
        }
    }
    break;

  case TTN_GETDISPINFO:
    {
      // convert mouse position to item/subitem
      LVHITTESTINFO lvHitTestInfo;
      lvHitTestInfo.flags = LVHT_ONITEM;
      GetCursorPos(&lvHitTestInfo.pt);
      ::ScreenToClient(hWndListView, &lvHitTestInfo.pt);
      ListView_SubItemHitTest(hWndListView, &lvHitTestInfo);

      int iRow = lvHitTestInfo.iItem;
      int iCol = lvHitTestInfo.iSubItem;
      if (iRow < 0)
        return false;

#if DEBUG
      Log (LOG_BABBLE) << "TTN_GETDISPINFO " << iRow << " " << iCol << endLog;
#endif

      // get the tooltip text for that item/subitem
      static StringCache tooltip;
      tooltip = "";
      if (contents)
        tooltip = (*contents)[iRow]->get_tooltip(iCol);

      // set the tooltip text
      NMTTDISPINFO *pNmTTDispInfo = (NMTTDISPINFO *)pNmHdr;
      pNmTTDispInfo->lpszText = tooltip;
      pNmTTDispInfo->hinst = NULL;
      pNmTTDispInfo->uFlags = 0;

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

int
ListView::popup_menu(int iRow, int iCol, POINT p)
{
  int update = 0;
  // construct menu
  HMENU hMenu = CreatePopupMenu();

  MENUITEMINFO mii;
  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_ID;
  mii.fType = MFT_STRING;

  ActionList *al = (*contents)[iRow]->get_actions(iCol);

  Actions::iterator i;
  int j = 1;
  for (i = al->list.begin (); i != al->list.end (); ++i, ++j)
    {
      BOOL res;
      mii.dwTypeData = (char *)i->name.c_str();
      mii.fState = (i->selected ? MFS_CHECKED : MFS_UNCHECKED |
                    i->enabled ? MFS_ENABLED : MFS_DISABLED);
      mii.wID = j;

      res = InsertMenuItem(hMenu, -1, TRUE, &mii);
      if (!res) Log (LOG_BABBLE) << "InsertMenuItem failed " << endLog;
    }

  int id = TrackPopupMenu(hMenu,
                          TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_NOANIMATION,
                          p.x, p.y, 0, hWndListView, NULL);

  // Inform the item of the menu choice
  if (id)
    update = (*contents)[iRow]->do_action(iCol, al->list[id-1].id);

  DestroyMenu(hMenu);
  delete al;

  return update;
}
