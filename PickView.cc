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

#include "PickView.h"
#include <commctrl.h>
#include "PickPackageLine.h"
#include "PickCategoryLine.h"
#include "package_db.h"
#include "package_version.h"
#include "dialog.h"
#include "resource.h"


static PickView::Header pkg_headers[] = {
  {"Current", 7, 0, 0},
  {"New", 3, 0, 0},
  {"Bin?", 4, 0, 0},
  {"Src?", 4, 0, 0},
  {"Category", 8, 0, 0},
  {"Package", 7, 0, 0},
  {0, 0, 0, 0}
};

static PickView::Header cat_headers[] = {
  {"Category", 8, 0, 0},
  {"Current", 7, 0, 0},
  {"New", 3, 0, 0},
  {"Bin?", 4, 0, 0},
  {"Src?", 4, 0, 0},
  {"Package", 7, 0, 0},
  {0, 0, 0, 0}
};

// PickView:: views
const PickView::views PickView::views::Unknown (0);
const PickView::views PickView::views::PackageFull (1);
const PickView::views PickView::views::Package = PickView::views (2);
const PickView::views PickView::views::Category (3);

// DoInsertItem - inserts an item into a header control.
// Returns the index of the new item.
// hwndHeader - handle to the header control.
// iInsertAfter - index of the previous item.
// nWidth - width of the new item.
// lpsz - address of the item string.
static int
DoInsertItem (HWND hwndHeader, int iInsertAfter, int nWidth, LPSTR lpsz)
{
  HDITEM hdi;
  int index;

  hdi.mask = HDI_TEXT | HDI_FORMAT | HDI_WIDTH;
  hdi.pszText = lpsz;
  hdi.cxy = nWidth;
  hdi.cchTextMax = lstrlen (hdi.pszText);
  hdi.fmt = HDF_LEFT | HDF_STRING;

  index = SendMessage (hwndHeader, HDM_INSERTITEM,
                       (WPARAM) iInsertAfter, (LPARAM) & hdi);

  return index;
}



void
PickView::set_headers ()
{
  if (view_mode == views::Unknown)
    return;
  if (view_mode == views::PackageFull ||
      view_mode == views::Package)
    {
      headers = pkg_headers;
      current_col = 0;
      new_col = 1;
      bintick_col = new_col + 1;
      srctick_col = bintick_col + 1;
      cat_col = srctick_col + 1;
      pkg_col = cat_col + 1;
      last_col = pkg_col;
    }
  else if (view_mode == views::Category)
    {
      headers = cat_headers;
      current_col = 1;
      new_col = current_col + 1;
      bintick_col = new_col + 1;
      srctick_col = bintick_col + 1;
      cat_col = 0;
      pkg_col = srctick_col + 1;
      last_col = pkg_col;
    }
  else
    return;
  while (int n = SendMessage (listheader, HDM_GETITEMCOUNT, 0, 0))
    {
      SendMessage (listheader, HDM_DELETEITEM, n - 1, 0);
    }
  int i;
  for (i = 0; i <= last_col; i++)
    DoInsertItem (listheader, i, headers[i].width, (char *) headers[i].text);
}

void
PickView::note_width (PickView::Header *hdrs, HDC dc, String const &string, int addend,
            int column)
{
  if (!string.size())
    {
      if (hdrs[column].width < addend)
        hdrs[column].width = addend;
      return;
    }
  SIZE s;
  GetTextExtentPoint32 (dc, string.cstr_oneuse(), string.size(), &s);
  if (hdrs[column].width < s.cx + addend)
    hdrs[column].width = s.cx + addend;
}

void
PickView::set_view_mode (PickView::views _mode)
{
  view_mode = _mode;
  set_headers ();
}

const char *
PickView::mode_caption ()
{
  return view_mode.caption ();
}

const char *
PickView::views::caption ()
{
  switch (_value)
    {
    case 1:
      return "Full";
    case 2:
      return "Partial";
    case 3:
      return "Category";
    default:
      return "";
    }
}

void
PickView::insert_pkg (packagemeta & pkg)
{
  if (view_mode != views::Category)
    {
      PickLine & line = *new PickPackageLine (*this, pkg);
      contents.insert (line);
    }
  else
    {
      for (size_t x = 1; x <= pkg.Categories.number (); x++)
        {
          Category & cat = pkg.Categories[x]->key;
          // Special case - yuck
          if (cat == Category ("All"))
            continue;
          PickCategoryLine & catline = *new PickCategoryLine (*this, cat, 1);
          PickLine & line = *new PickPackageLine(*this, pkg);
          catline.insert (line);
          contents.insert (catline);
        }
    }
}

void
PickView::insert_category (Category * cat, bool collapsed)
{
  if (*cat == Category ("All"))
    return;
  PickCategoryLine & catline = *new PickCategoryLine (*this, *cat, 1, collapsed);
  for (CategoryPackage * catpkg = cat->packages; catpkg;
       catpkg = catpkg->next)
    {

      PickLine & line = *new PickPackageLine (*this, *catpkg->pkg);
      catline.insert (line);
    }
  contents.insert (catline);
}

void
PickView::clear_view (void)
{
  contents.empty ();
  if (view_mode == views::Unknown)
    return;
  if (view_mode == views::PackageFull ||
      view_mode == views::Package)
    contents.ShowLabel (false);
  else if (view_mode == views::Category)
    contents.ShowLabel ();
}

PickView::views&
PickView::views::operator++ ()
{
  ++_value;
  if (_value > Category._value)
    _value = 1;
  return *this;
}

int
PickView::click (int row, int x)
{
  return contents.click (0, row, x);
}


void
PickView::scroll (HWND hwnd, int which, int *var, int code)
{
  SCROLLINFO si;
  si.cbSize = sizeof (si);
  si.fMask = SIF_ALL;
  GetScrollInfo (hwnd, which, &si);

  switch (code)
    {
    case SB_THUMBTRACK:
      si.nPos = si.nTrackPos;
      break;
    case SB_THUMBPOSITION:
      break;
    case SB_BOTTOM:
      si.nPos = si.nMax;
      break;
    case SB_TOP:
      si.nPos = 0;
      break;
    case SB_LINEDOWN:
      si.nPos += row_height;
      break;
    case SB_LINEUP:
      si.nPos -= row_height;
      break;
    case SB_PAGEDOWN:
      si.nPos += si.nPage * 9 / 10;
      break;
    case SB_PAGEUP:
      si.nPos -= si.nPage * 9 / 10;
      break;
    }

  if ((int) si.nPos < 0)
    si.nPos = 0;
  if (si.nPos + si.nPage > (unsigned int) si.nMax)
    si.nPos = si.nMax - si.nPage;

  si.fMask = SIF_POS;
  SetScrollInfo (hwnd, which, &si, TRUE);

  int ox = scroll_ulc_x;
  int oy = scroll_ulc_y;
  *var = si.nPos;

  RECT cr, sr;
  GetClientRect (hwnd, &cr);
  sr = cr;
  sr.top += header_height;
  UpdateWindow (hwnd);
  ScrollWindow (hwnd, ox - scroll_ulc_x, oy - scroll_ulc_y, &sr, &sr);
  /*
     sr.bottom = sr.top;
     sr.top = cr.top;
     ScrollWindow (hwnd, ox - scroll_ulc_x, 0, &sr, &sr);
   */
  if (ox - scroll_ulc_x)
    {
      GetClientRect (listheader, &cr);
      sr = cr;
//  UpdateWindow (htmp);
      MoveWindow (listheader, -scroll_ulc_x, 0,
                  headers[last_col].x +
                  headers[last_col].width, header_height, TRUE);
    }
  UpdateWindow (hwnd);
}

void
PickView::init_headers (HDC dc)
{
  int i;

  for (i = 0; headers[i].text; i++)
    {
      headers[i].width = 0;
      headers[i].x = 0;
    }

  for (i = 0; headers[i].text; i++)
    note_width (headers, dc, headers[i].text, HMARGIN, i);
  /* src checkbox */
  note_width (headers, dc, 0, HMARGIN + 11, bintick_col);
  note_width (headers, dc, 0, HMARGIN + 11, srctick_col);
  packagedb db;
  for (size_t n = 1; n <= db.categories.number (); n++)
      note_width (headers, dc, String ("+ ")+db.categories[n]->name, HMARGIN, cat_col);
  for (size_t n = 1; n <= db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      if (pkg.installed)
        note_width (headers, dc, pkg.installed->Canonical_version (),
                    HMARGIN, current_col);
      for (size_t n = 1; n <= pkg.versions.number (); n++)
        if (pkg.versions[n] != pkg.installed)
          note_width (headers, dc,
                      pkg.versions[n]->Canonical_version (),
                      NEW_COL_SIZE_SLOP + HMARGIN, new_col);
      String s = pkg.name;
      if (pkg.SDesc ().size())
	s += String (": ") + pkg.SDesc ();
      note_width (headers, dc, s, HMARGIN, pkg_col);
    }
  note_width (headers, dc, "keep", NEW_COL_SIZE_SLOP + HMARGIN, new_col);
  note_width (headers, dc, "uninstall", NEW_COL_SIZE_SLOP + HMARGIN, new_col);

  headers[0].x = 0;
  for (i = 1; i <= last_col; i++)
    headers[i].x = headers[i - 1].x + headers[i - 1].width;
}


PickView::PickView (views _mode, HWND lv, Category &cat) : deftrust (TRUST_UNKNOWN),
contents (*this, cat, 0, false, true), listview (lv)
{

  HDC dc = GetDC (listview);
  sysfont = GetStockObject (DEFAULT_GUI_FONT);
  SelectObject (dc, sysfont);
  GetTextMetrics (dc, &tm);

  bitmap_dc = CreateCompatibleDC (dc);
  bm_spin = LoadImage (hinstance, MAKEINTRESOURCE (IDB_SPIN), IMAGE_BITMAP, 0, 0, 0);
  bm_rtarrow = LoadImage (hinstance, MAKEINTRESOURCE (IDB_RTARROW), IMAGE_BITMAP,
                              0, 0, 0);

  bm_checkyes = LoadImage (hinstance, MAKEINTRESOURCE (IDB_CHECK_YES), IMAGE_BITMAP,
                               0, 0, 0);
  bm_checkno = LoadImage (hinstance, MAKEINTRESOURCE (IDB_CHECK_NO), IMAGE_BITMAP,
                              0, 0, 0);
  bm_checkna = LoadImage (hinstance, MAKEINTRESOURCE (IDB_CHECK_NA), IMAGE_BITMAP,
                              0, 0, 0);

  row_height = (tm.tmHeight + tm.tmExternalLeading + ROW_MARGIN);
  int
    irh =
    tm.
    tmExternalLeading +
    tm.
    tmDescent +
    11 +
    ROW_MARGIN;
  if (row_height < irh)
    row_height = irh;

  RECT rcParent;
  HDLAYOUT hdl;
  WINDOWPOS wp;

  // Ensure that the common control DLL is loaded, and then create
  // the header control.
  INITCOMMONCONTROLSEX controlinfo =
  {
  sizeof (INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx (&controlinfo);

  if ((listheader = CreateWindowEx (0, WC_HEADER, (LPCTSTR) NULL,
                                    WS_CHILD | WS_BORDER | CCS_NORESIZE |
                                    // | HDS_BUTTONS
                                    HDS_HORZ, 0, 0, 0, 0, listview,
                                    (HMENU) IDC_CHOOSE_LISTHEADER, hinstance,
                                    (LPVOID) NULL)) == NULL)
    // FIXME: throw an exception
    exit (10);

  // Retrieve the bounding rectangle of the parent window's
  // client area, and then request size and position values
  // from the header control.
  GetClientRect (listview, &rcParent);

  hdl.prc = &rcParent;
  hdl.pwpos = &wp;
  if (!SendMessage (listheader, HDM_LAYOUT, 0, (LPARAM) & hdl))
    // FIXME: throw an exception
    exit (11);


  // Set the size, position, and visibility of the header control.
  SetWindowPos (listheader, wp.hwndInsertAfter, wp.x, wp.y,
                wp.cx, wp.cy, wp.flags | SWP_SHOWWINDOW);

  header_height = wp.cy;

  view_mode = PickView::views::Package;
  set_headers ();
  init_headers (dc);
  view_mode = PickView::views::Category;
  set_headers ();
  init_headers (dc);

  view_mode = _mode;
  set_headers ();

  ReleaseDC (lv, dc);
}

PickView::~PickView()
{
}
