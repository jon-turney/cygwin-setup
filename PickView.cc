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
#include <algorithm>
/* For 'source' */
#include "state.h"
#include "LogSingleton.h"

using namespace std;

static PickView::Header pkg_headers[] = {
  {"Current", 7, 0, 0},
  {"New", 3, 0, 0},
  {"Bin?", 4, 0, 0},
  {"Src?", 4, 0, 0},
  {"Categories", 10, 0, 0},
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
const PickView::views PickView::views::Package (2);
const PickView::views PickView::views::PackageKeeps (3);
const PickView::views PickView::views::PackageSkips = PickView::views (4);
const PickView::views PickView::views::Category (5);

ATOM PickView::WindowClassAtom = 0;

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
      view_mode == views::Package ||
      view_mode == views::PackageKeeps ||
      view_mode == views::PackageSkips)
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
      return "Up To Date";
    case 4:
      return "Not Installed";
    case 5:
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
      for (set <String, String::caseless>::const_iterator x
	   = pkg.categories.begin (); x != pkg.categories.end (); ++x)
        {
	  packagedb db;
	  // Special case - yuck
	  if (x->casecompare ("All") == 0)
	    continue;

	  PickCategoryLine & catline = 
	    *new PickCategoryLine (*this,* db.categories.find (*x), 1);
	  PickLine & line = *new PickPackageLine(*this, pkg);
	  catline.insert (line);
	  contents.insert (catline);
        }
    }
}

void
PickView::insert_category (Category * cat, bool collapsed)
{
  // Urk, special case
  if (cat->first.casecompare ("All") == 0)
    return;
  PickCategoryLine & catline = *new PickCategoryLine (*this, *cat, 1, collapsed);
  for (vector <packagemeta *>::iterator i = cat->second.begin ();
       i != cat->second.end () ; ++i)
    {
      PickLine & line = *new PickPackageLine (*this, **i);
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
      view_mode == views::Package ||
      view_mode == views::PackageKeeps ||
      view_mode == views::PackageSkips)
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
  si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
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
  ::GetClientRect (hwnd, &cr);
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
      ::GetClientRect (listheader, &cr);
      sr = cr;
//  UpdateWindow (htmp);
      ::MoveWindow (listheader, -scroll_ulc_x, 0,
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
  for (packagedb::categoriesType::iterator n = packagedb::categories.begin();
       n != packagedb::categories.end(); ++n)
    note_width (headers, dc, String ("+ ")+n->first, HMARGIN, cat_col);
  for (vector <packagemeta *>::iterator n = db.packages.begin ();
       n != db.packages.end (); ++n)
    {
      packagemeta & pkg = **n;
      if (pkg.installed)
        note_width (headers, dc, pkg.installed.Canonical_version (),
                    HMARGIN, current_col);
      for (set<packageversion>::iterator i=pkg.versions.begin();
	   i != pkg.versions.end(); ++i)
        if (*i != pkg.installed)
          note_width (headers, dc,
                      i->Canonical_version (),
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


PickView::PickView (Category &cat) : deftrust (TRUST_UNKNOWN),
contents (*this, cat, 0, false, true)
{
}

void
PickView::init(views _mode)
{
  HDC dc = GetDC (GetHWND());
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
                                    HDS_HORZ, 0, 0, 0, 0, GetHWND(),
                                    (HMENU) IDC_CHOOSE_LISTHEADER, hinstance,
                                    (LPVOID) NULL)) == NULL)
    // FIXME: throw an exception
    exit (10);

  // Retrieve the bounding rectangle of the parent window's
  // client area, and then request size and position values
  // from the header control.
  ::GetClientRect (GetHWND(), &rcParent);

  hdl.prc = &rcParent;
  hdl.pwpos = &wp;
  if (!SendMessage (listheader, HDM_LAYOUT, 0, (LPARAM) & hdl))
    // FIXME: throw an exception
    exit (11);

  // Set the font of the listheader, but don't redraw, because its not shown
  // yet.This message does not return a value, so we are not checking it as we
  // do above.
  SendMessage (listheader, WM_SETFONT, (WPARAM) sysfont, FALSE);

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

  ReleaseDC (GetHWND(), dc);
}

PickView::~PickView()
{
}

bool PickView::registerWindowClass ()
{
  if (WindowClassAtom != 0)
    return true;

  // We're not registered yet
  WNDCLASSEX wc;

  wc.cbSize = sizeof (wc);
  // Some sensible style defaults
  wc.style = CS_HREDRAW | CS_VREDRAW;
  // Our default window procedure.  This replaces itself
  // on the first call with the simpler Window::WindowProcReflector().
  wc.lpfnWndProc = Window::FirstWindowProcReflector;
  // No class bytes
  wc.cbClsExtra = 0;
  // One pointer to REFLECTION_INFO in the extra window instance bytes
  wc.cbWndExtra = 4;
  // The app instance
  wc.hInstance = hinstance; //GetInstance ();
  // Use a bunch of system defaults for the GUI elements
  wc.hIcon = LoadIcon (0, IDI_APPLICATION);
  wc.hIconSm = NULL;
  wc.hCursor = LoadCursor (0, IDC_ARROW);
  wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
  // No menu
  wc.lpszMenuName = NULL;
  // We'll get a little crazy here with the class name
  wc.lpszClassName = "listview";

  // All set, try to register
  WindowClassAtom = RegisterClassEx (&wc);
  if (WindowClassAtom == 0)
    log (LOG_BABBLE) << "Failed to register listview " << GetLastError () << endLog;
  return WindowClassAtom != 0;
}

LRESULT CALLBACK
PickView::list_vscroll (HWND hwnd, HWND hctl, UINT code, int pos)
{
  scroll (hwnd, SB_VERT, &scroll_ulc_y, code);
  return 0;
}

LRESULT CALLBACK
PickView::list_hscroll (HWND hwnd, HWND hctl, UINT code, int pos)
{
  scroll (hwnd, SB_HORZ, &scroll_ulc_x, code);
  return 0;
}

LRESULT CALLBACK
PickView::list_click (HWND hwnd, BOOL dblclk, int x, int y, UINT hitCode)
{
  int row, refresh;

  if (contents.itemcount () == 0)
    return 0;

  if (y < header_height)
    return 0;
  x += scroll_ulc_x;
  y += scroll_ulc_y - header_height;

  row = (y + ROW_MARGIN / 2) / row_height;

  if (row < 0 || row >= contents.itemcount ())
    return 0;

  refresh = click (row, x);

  // XXX we need a method to queryt he database to see if more
  // than just one package has changed! Until then...
#if 0
  if (refresh)
    {
#endif
      RECT r;
      ::GetClientRect (GetHWND(), &r);
      SCROLLINFO si;
      memset (&si, 0, sizeof (si));
      si.cbSize = sizeof (si);
      si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;    /* SIF_RANGE was giving strange behaviour */
      si.nMin = 0;

      si.nMax = contents.itemcount () * row_height;
      si.nPage = r.bottom - header_height;

      /* if we are under the minimum display count ,
       * set the offset to 0
       */
      if ((unsigned int) si.nMax <= si.nPage)
        scroll_ulc_y = 0;
      si.nPos = scroll_ulc_y;

      SetScrollInfo (GetHWND(), SB_VERT, &si, TRUE);

      InvalidateRect (GetHWND(), &r, TRUE);
#if 0
    }
  else
    {
      RECT rect;
      rect.left =
        headers[new_col].x - scroll_ulc_x;
      rect.right =
        headers[src_col + 1].x - scroll_ulc_x;
      rect.top =
        header_height + row * row_height -
        scroll_ulc_y;
      rect.bottom = rect.top + row_height;
      InvalidateRect (hwnd, &rect, TRUE);
    }
#endif
  return 0;
}

/*
 * LRESULT CALLBACK
 * PickView::listview_proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
 */
LRESULT
PickView::WindowProc (UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_HSCROLL:
      return HANDLE_WM_HSCROLL (GetHWND(), wParam, lParam, list_hscroll);
    case WM_VSCROLL:
      return HANDLE_WM_VSCROLL (GetHWND(), wParam, lParam, list_vscroll);
    case WM_LBUTTONDOWN:
      return HANDLE_WM_LBUTTONDOWN (GetHWND(), wParam, lParam, list_click);
    case WM_PAINT:
      paint (GetHWND());
      return 0;
    case WM_NOTIFY:
      {
 // pnmh = (LPNMHDR) lParam
 LPNMHEADER phdr = (LPNMHEADER) lParam;
 switch (phdr->hdr.code)
      {
      case HDN_ITEMCHANGED:
        if (phdr->hdr.hwndFrom == ListHeader ())
        {
        if (phdr->pitem && phdr->pitem->mask & HDI_WIDTH)
        headers[phdr->iItem].width = phdr->pitem->cxy;
      for (int i = 1; i <= last_col; i++)
         headers[i].x =
          headers[i - 1].x + headers[i - 1].width;
     RECT r;
        ::GetClientRect (GetHWND(), &r);
        SCROLLINFO si;
     si.cbSize = sizeof (si);
       si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
      GetScrollInfo (GetHWND(), SB_HORZ, &si);
        int oldMax = si.nMax;
      si.nMax =
        headers[last_col].x +
        headers[last_col].width;
       if (si.nTrackPos && oldMax > si.nMax)
        si.nTrackPos += si.nMax - oldMax;
        si.nPage = r.right;
        SetScrollInfo (GetHWND(), SB_HORZ, &si, TRUE);
      InvalidateRect (GetHWND(), &r, TRUE);
       if (si.nTrackPos && oldMax > si.nMax)
        scroll (GetHWND(), SB_HORZ, &scroll_ulc_x,
                 SB_THUMBTRACK);
       }
        break;
   default:
     break;
   }
      }
    default:
      return DefWindowProc (GetHWND(), message, wParam, lParam);
    }
}

void
PickView::paint (HWND hwnd)
{
  HDC hdc;
  PAINTSTRUCT ps;
  int x, y;

  hdc = BeginPaint (hwnd, &ps);

  SelectObject (hdc, sysfont);
  SetBkColor (hdc, GetSysColor (COLOR_WINDOW));
  SetTextColor (hdc, GetSysColor (COLOR_WINDOWTEXT));

  RECT cr;
  ::GetClientRect (hwnd, &cr);

  x = cr.left - scroll_ulc_x;
  y = cr.top - scroll_ulc_y + header_height;

  IntersectClipRect (hdc, cr.left, cr.top + header_height, cr.right,
             cr.bottom);

  contents.paint (hdc, x, y, 0, (get_view_mode () ==
                     PickView::views::Category) ? 0 : 1);

  if (contents.itemcount () == 0)
    {
      static const char *msg = "Nothing to Install/Update";
      if (source == IDC_SOURCE_DOWNLOAD)
  msg = "Nothing to Download";
      TextOut (hdc, HMARGIN, header_height, msg, strlen (msg));
    }

  EndPaint (hwnd, &ps);
}


bool 
PickView::Create (Window * parent, DWORD Style, RECT *r)
{

  // First register the window class, if we haven't already
  if (!registerWindowClass ())
    {
      // Registration failed
      return false;
    }

    // Save our parent, we'll probably need it eventually.
  setParent(parent);

  // Create the window instance
  CreateWindowEx (
                   // Extended Style
                   WS_EX_CLIENTEDGE,
                   "listview",   //MAKEINTATOM(WindowClassAtom),     // window class atom (name)
                   "listviewwindow", // no title-bar string yet
                // Style bits
                  Style,
                 r ? r->left : CW_USEDEFAULT, r ? r->top : CW_USEDEFAULT, 
                 r? r->right - r->left + 1 : CW_USEDEFAULT, 
                 r? r->bottom - r->top + 1 :CW_USEDEFAULT,
                   // Parent Window 
                  parent == NULL ? (HWND) NULL : parent->GetHWND (),
                   // use class menu 
                (HMENU) MAKEINTRESOURCE (IDC_CHOOSE_LIST),
                  // The application instance 
                   GetInstance (),
                // The this ptr, which we'll use to set up the WindowProc reflection.
                  reinterpret_cast<void *>((Window *)this));
  if (GetHWND() == NULL)
    {
      log (LOG_BABBLE) << "Failed to create PickView " << GetLastError () << endLog;
      return false;
    }

  return true;
}

void
PickView::defaultTrust (trusts trust)
{
  this->deftrust = trust;
  packagedb db;
  for (vector <packagemeta *>::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta & pkg = **i;
      if (pkg.installed
     || pkg.categories.find ("Base") != pkg.categories.end ()
   || pkg.categories.find ("Misc") != pkg.categories.end ())
    {
    pkg.desired = pkg.trustp (trust);
      if (pkg.desired)
     pkg.desired.pick (pkg.desired.accessible() 
                      && pkg.desired != pkg.installed);
    }
      else
   pkg.desired = packageversion ();
    }
  RECT r;
  ::GetClientRect (this->GetHWND(), &r);
  InvalidateRect (this->GetHWND(), &r, TRUE);
  // and then do the same for categories with no packages.
  for (packagedb::categoriesType::iterator n = packagedb::categories.begin();
       n != packagedb::categories.end(); ++n)
    if (!n->second.size())
      {
    log (LOG_BABBLE) << "Removing empty category " << n->first << endLog;
        packagedb::categories.erase (n++);
      }
}
