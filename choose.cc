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
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

/* The purpose of this file is to let the user choose which packages
   to install, and which versions of the package when more than one
   version is provided.  The "trust" level serves as an indication as
   to which version should be the default choice.  At the moment, all
   we do is compare with previously installed packages to skip any
   that are already installed (by setting the action to ACTION_SAME).
   While the "trust" stuff is supported, it's not really implemented
   yet.  We always prefer the "current" option.  In the future, this
   file might have a user dialog added to let the user choose to not
   install packages, or to install packages that aren't installed by
   default. */

static char *cvsid = "\n%%% $Id$\n";

#include "win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <ctype.h>

#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "ini.h"
#include "concat.h"
#include "msg.h"
#include "log.h"
#include "find.h"
#include "filemanip.h"
#include "mount.h"

#include "port.h"

#define HMARGIN	10
#define ROW_MARGIN	5
#define ICON_MARGIN	4
#define NEW_COL_SIZE_SLOP (ICON_MARGIN + 11)

#define CHECK_SIZE	11

static int initialized = 0;

static int full_list = 0;

static int scroll_ulc_x, scroll_ulc_y;

static HWND lv, nextbutton, choose_inst_text;
static TEXTMETRIC tm;
static int header_height;
static HANDLE sysfont;
static int row_height;
static HANDLE bm_spin, bm_rtarrow, bm_checkyes, bm_checkno, bm_checkna;
static HDC bitmap_dc;

static struct
  {
    char *text;
    int slen;
    int width;
    int x;
  }
headers[] = {
  { "Current", 7, 0, 0 },
#define CURRENT_COL 0
  { "New", 3, 0, 0 },
#define NEW_COL 1
  { "Src?", 4, 0, 0 },
#define SRC_COL 2
  { "Package", 7, 0, 0 },
#define PACKAGE_COL 3
  { 0, 0, 0, 0 }
};
#define NUM_COLUMNS (sizeof (headers) / (sizeof (headers[0])) - 1)

int *package_indexes, nindexes;

static bool
isinstalled (Package *pkg, int trust)
{
  if (source == IDC_SOURCE_DOWNLOAD)
    return pkg->info[pkg->installed_ix].install_exists < 0;
  else
    return pkg->installed && pkg->info[trust].version &&
	   strcmp (pkg->installed->version, pkg->info[trust].version) == 0;
}

/* Set the next action given a current action.  */
static void
set_action (Package *pkg, bool preinc)
{
  pkg->srcpicked = 0;
  if (!pkg->action || preinc)
    ((int) pkg->action)++;

  /* Exercise the action state machine. */
  for (;; ((int) pkg->action)++)
    switch (pkg->action)
      {
      case ACTION_ERROR:
      case ACTION_UNKNOWN:
	pkg->action = (actions) (ACTION_CURR - 1);
	break;
      case ACTION_LAST:
	pkg->action = ACTION_PREV;
	/* fall through intentionally */
      case ACTION_PREV:
      case ACTION_CURR:
      case ACTION_TEST:
	/* Try to find the next best action.  We may not have all of
	   prev, curr, or test but we should have at least one of those. */
	Info *inf;
	inf = pkg->info + pkg->action;
	if (inf->version && inf->install_exists)
	  {
	    if (isinstalled (pkg, pkg->action))
	      (int) pkg->action += ACTION_SAME;
	    return;
	  }
	break;
      /* ACTION_SAME_* are used when the installed version is the same
	 as the given action. */
      case ACTION_SAME_CURR:
      case ACTION_SAME_TEST:
	(int) pkg->action -= ACTION_SAME + 1;	/* revert to ACTION_CURR, etc. */
	break;
      case ACTION_SAME_PREV:
	pkg->action = ACTION_UNINSTALL;
      /* Fall through intentionally */
      case ACTION_UNINSTALL:
	if (pkg->installed)
	  return;
      case ACTION_REDO:
	if (pkg->installed)
	  {
	    pkg->trust = pkg->installed_ix;
	    return;
	  }
      case ACTION_SRC_ONLY:
	if (pkg->installed && pkg->installed->source_exists)
	  return;
	break;
      case ACTION_SAME_LAST:
	pkg->action = ACTION_SKIP;
	/* Fall through intentionally */
      case ACTION_SKIP:
	return;
      default:
	log (0, "should never get here %d\n", pkg->action);
      }
}

/* Return an appropriate caption given the current action. */
const char *
choose_caption (Package *pkg)
{
  set_action (pkg, 0);
  switch (pkg->action)
    {
    case ACTION_PREV:
    case ACTION_CURR:
    case ACTION_TEST:
      pkg->trust = (trusts) pkg->action;
      return pkg->info[pkg->trust].version;
    case ACTION_SAME_PREV:
    case ACTION_SAME_CURR:
    case ACTION_SAME_TEST:
      return "Keep";
    case ACTION_UNINSTALL:
      return "Uninstall";
    case ACTION_REDO:
      return "Reinstall";
    case ACTION_SRC_ONLY:
      if (pkg->installed && pkg->installed->source_exists)
	return "Redo Source";
      else
	return "Source";
    case ACTION_SKIP:
      return "Skip";
    }
    return "???";
}

static void
paint (HWND hwnd)
{
  HDC hdc;
  PAINTSTRUCT ps;
  int x, y, i, j, ii;

  hdc = BeginPaint (hwnd, &ps);

  SelectObject (hdc, sysfont);
  SetBkColor (hdc, GetSysColor (COLOR_WINDOW));

  RECT cr;
  GetClientRect (hwnd, &cr);

  POINT p;

  x = cr.left - scroll_ulc_x;
  y = cr.top - scroll_ulc_y + header_height;


  for (i = 0; headers[i].text; i++)
    {
      TextOut (hdc, x + headers[i].x, 3, headers[i].text, headers[i].slen);
      MoveToEx (hdc, x + headers[i].x, header_height-3, &p);
      LineTo (hdc, x + headers[i].x + headers[i].width, header_height-3);
    }

  IntersectClipRect (hdc, cr.left, cr.top + header_height, cr.right, cr.bottom);

  for (ii = 0; ii < nindexes; ii++)
    {
      i = package_indexes[ii];
      Package *pkg = package + i;
      int r = y + ii * row_height;
      int by = r + tm.tmHeight - 11;
      if (pkg->installed)
	{
	  TextOut (hdc, x + headers[CURRENT_COL].x, r,
		   pkg->installed->version, strlen (pkg->installed->version));
	  SelectObject (bitmap_dc, bm_rtarrow);
	  BitBlt (hdc, x + headers[CURRENT_COL].x + headers[0].width + ICON_MARGIN/2 + HMARGIN/2, by,
		  11, 11, bitmap_dc, 0, 0, SRCCOPY);
	}

      const char *s = choose_caption (pkg);
      TextOut (hdc, x + headers[NEW_COL].x + NEW_COL_SIZE_SLOP, r,
	       s, strlen (s));
      SelectObject (bitmap_dc, bm_spin);
      BitBlt (hdc, x + headers[NEW_COL].x, by, 11, 11,
	      bitmap_dc, 0, 0, SRCCOPY);

      HANDLE check_bm;
      if (pkg->srcpicked)
	check_bm = bm_checkyes;
      else
	check_bm = bm_checkno;

      SelectObject (bitmap_dc, check_bm);
      BitBlt (hdc, x + headers[SRC_COL].x, by, 11, 11,
	      bitmap_dc, 0, 0, SRCCOPY);

      if (package[i].sdesc)
	s = package[i].sdesc;
      else
	s = package[i].name;
      TextOut (hdc, x + headers[PACKAGE_COL].x, r, s, strlen (s));
    }

  if (nindexes == 0)
    {
      static char *msg = "Nothing to Install/Update";
      if (source == IDC_SOURCE_DOWNLOAD)
	msg = "Nothing to Download";
      TextOut (hdc, HMARGIN, header_height, msg, strlen (msg));
    }

  EndPaint (hwnd, &ps);
}

static void
scroll_common (HWND hwnd, int which, int *var, int code)
{
  int v = *var;

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
      si.nPos += si.nPage * 9/10;
      break;
    case SB_PAGEUP:
      si.nPos -= si.nPage * 9/10;
      break;
    }

  if ((int)si.nPos < 0)
    si.nPos = 0;
  if (si.nPos + si.nPage > si.nMax)
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
  ScrollWindow (hwnd, ox - scroll_ulc_x, oy - scroll_ulc_y, &sr, &sr);
  sr.bottom = sr.top;
  sr.top = cr.top;
  ScrollWindow (hwnd, ox - scroll_ulc_x, 0, &sr, &sr);
}

static LRESULT CALLBACK
list_vscroll (HWND hwnd, HWND hctl, UINT code, int pos)
{
  scroll_common (hwnd, SB_VERT, &scroll_ulc_y, code);
}

static LRESULT CALLBACK
list_hscroll (HWND hwnd, HWND hctl, UINT code, int pos)
{
  scroll_common (hwnd, SB_HORZ, &scroll_ulc_x, code);
}

static LRESULT CALLBACK
list_click (HWND hwnd, BOOL dblclk, int x, int y, UINT hitCode)
{
  int r;

  if (nindexes == 0)
    return 0;

  if (y < header_height)
    return 0;
  x += scroll_ulc_x;
  y += scroll_ulc_y - header_height;

  r = (y + ROW_MARGIN/2) / row_height;

  if (r < 0 || r >= nindexes)
    return 0;

  Package *pkg = package + package_indexes[r];

  if (x >= headers[NEW_COL].x - (HMARGIN / 2) && x <= headers[NEW_COL + 1].x - HMARGIN/2)
    set_action (pkg, 1);

  if (x >= headers[SRC_COL].x - HMARGIN/2 && x <= headers[SRC_COL + 1].x - HMARGIN/2)
    pkg->srcpicked ^= 1;

  RECT rect;
  rect.left = headers[NEW_COL].x - scroll_ulc_x;
  rect.right = headers[SRC_COL + 1].x - scroll_ulc_x;
  rect.top = header_height + r * row_height - scroll_ulc_y;
  rect.bottom = rect.top + row_height;
  InvalidateRect (hwnd, &rect, TRUE);
}

static LRESULT CALLBACK
listview_proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_HSCROLL:
      return HANDLE_WM_HSCROLL (hwnd, wParam, lParam, list_hscroll);
    case WM_VSCROLL:
      return HANDLE_WM_VSCROLL (hwnd, wParam, lParam, list_vscroll);
    case WM_LBUTTONDOWN:
      return HANDLE_WM_LBUTTONDOWN (hwnd, wParam, lParam, list_click);
    case WM_PAINT:
      paint (hwnd);
      return 0;
    default:
      return DefWindowProc (hwnd, message, wParam, lParam);
    }
}

static void
register_windows (HINSTANCE hinst)
{
  WNDCLASSEX wcex;
  static int done = 0;

  if (done)
    return;
  done = 1;

  memset (&wcex, 0, sizeof (wcex));
  wcex.cbSize = sizeof (WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = listview_proc;
  wcex.hInstance = hinst;
  wcex.hIcon = LoadIcon (0, IDI_APPLICATION);
  wcex.hCursor = LoadCursor (0, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
  wcex.lpszClassName = "listview";

  RegisterClassEx (&wcex);
}

static void
note_width (HDC dc, char *string, int addend, int column)
{
  if (!string)
    return;
  SIZE s;
  GetTextExtentPoint32 (dc, string, strlen (string), &s);
  if (headers[column].width < s.cx + addend)
    headers[column].width = s.cx + addend;
}

static int
check_existence (Info *inf, int check_src)
{
  if (source == IDC_SOURCE_NETINST)
    return 1;
  if (source == IDC_SOURCE_CWD)
    return 0;  /* should have already been determined */
  if (check_src)
    return (inf->source && _access (inf->source, 0) == 0) ? -1 : 1;
  else
    return (inf->install && _access (inf->install, 0) == 0) ? -1 : 1;
  return 0;
}

/* Iterate over the package list, setting the "existence" flags
   for the source or binary. */
static void
set_existence ()
{
  for (Package *pkg = package; pkg->name; pkg++)
    if (!pkg->exclude)
      {
	int exists = 0;
	for (int t = 1; t < NTRUST; t++)
	  {
	    Info *inf = pkg->info + t;
	    if (inf->install_exists)
	      exists = 1;
	    else
	      exists |= inf->install_exists = check_existence (inf, 0);
	    if (inf->source_exists)
	      exists = 1;
	    else
	      exists |= inf->source_exists = check_existence (inf, 1);
	  }
	if (!exists)
	  pkg->exclude = EXCLUDE_NOT_FOUND;
      }
}

static void
default_trust (HWND h, trusts trust)
{
  int i, t, c;

  for (Package *pkg = package; pkg->name; pkg++)
    {
      pkg->action = (actions) trust;
      set_action (pkg, 0);
    }
  RECT r;
  GetClientRect (h, &r);
  InvalidateRect (h, &r, TRUE);
  if (nextbutton)
    SetFocus (nextbutton);
}

static void
set_full_list (HWND h, int isfull)
{
  int i;
  full_list = isfull;

  if (package_indexes == 0)
    package_indexes = (int *) malloc (npackages * sizeof (int));

  nindexes = 0;
  for (Package *pkg = package; pkg->name; pkg++)
    if (!pkg->exclude)
      {
	set_action (pkg, 0);
	if ((isfull || is_download_action (pkg)))
	  package_indexes[nindexes++] = pkg - package;
      }

  RECT r;
  GetClientRect (h, &r);
  SCROLLINFO si;
  memset (&si, 0, sizeof (si));
  si.cbSize = sizeof (si);
  si.fMask = SIF_ALL;
  si.nMin = 0;
  si.nMax = headers[2].x + headers[2].width + HMARGIN;
  si.nPage = r.right;
  SetScrollInfo (h, SB_HORZ, &si, TRUE);

  si.nMax = nindexes * row_height;
  si.nPage = r.bottom - header_height;
  SetScrollInfo (h, SB_VERT, &si, TRUE);

  scroll_ulc_x = scroll_ulc_y = 0;

  InvalidateRect (h, &r, TRUE);

  if (nextbutton)
    SetFocus (nextbutton);
}

static void
create_listview (HWND dlg, RECT *r)
{
  int i, t;
  lv = CreateWindowEx (WS_EX_CLIENTEDGE,
		       "listview",
		       "listviewwindow",
		       WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE,
		       r->left, r->top,
		       r->right-r->left + 1, r->bottom-r->top + 1,
		       dlg,
		       (HMENU) MAKEINTRESOURCE(IDC_CHOOSE_LIST),
		       hinstance,
		       0);
  ShowWindow (lv, SW_SHOW);

  for (i = 0; headers[i].text; i++)
    headers[i].width = 0;

  HDC dc = GetDC (lv);
  sysfont = GetStockObject (DEFAULT_GUI_FONT);
  SelectObject (dc, sysfont);
  GetTextMetrics (dc, &tm);
  header_height = tm.tmHeight + 5 + 3;

  bitmap_dc = CreateCompatibleDC (dc);

  row_height = (tm.tmHeight + tm.tmExternalLeading + ROW_MARGIN);
  int irh = tm.tmExternalLeading + tm.tmDescent + 11 + ROW_MARGIN;
  if (row_height < irh)
    row_height = irh;

  for (i = 0; headers[i].text; i++)
    note_width (dc, headers[i].text, 0, i);
  for (Package *pkg = package; pkg->name; pkg++)
    {
      if (pkg->installed)
	{
	  note_width (dc, pkg->installed->version, 0, CURRENT_COL);
	  note_width (dc, pkg->installed->version, NEW_COL_SIZE_SLOP, NEW_COL);
	}
      for (t = 1; t < NTRUST; t++)
	note_width (dc, pkg->info[t].version, NEW_COL_SIZE_SLOP, NEW_COL);
      note_width (dc, pkg->name, 0, PACKAGE_COL);
      note_width (dc, pkg->sdesc, 0, PACKAGE_COL);
    }
  note_width (dc, "keep", NEW_COL_SIZE_SLOP, NEW_COL);
  note_width (dc, "uninstall", NEW_COL_SIZE_SLOP, NEW_COL);

  headers[CURRENT_COL].x = HMARGIN/2;
  headers[NEW_COL].x = headers[CURRENT_COL].x + headers[CURRENT_COL].width + NEW_COL_SIZE_SLOP + HMARGIN;
  headers[SRC_COL].x = headers[NEW_COL].x + headers[NEW_COL].width + HMARGIN;
  headers[PACKAGE_COL].x = headers[SRC_COL].x + headers[SRC_COL].width + HMARGIN;

  default_trust (lv, TRUST_CURR);
  set_full_list (lv, full_list);
  static int ta[] = { IDC_CHOOSE_CURR, 0 };
  rbset (dlg, ta, IDC_CHOOSE_CURR);

  ReleaseDC (lv, dc);
}

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {
    case IDC_CHOOSE_PREV:
      default_trust (lv, TRUST_PREV);
      set_full_list (lv, full_list);
      break;
    case IDC_CHOOSE_CURR:
      default_trust (lv, TRUST_CURR);
      set_full_list (lv, full_list);
      break;
    case IDC_CHOOSE_EXP:
      default_trust (lv, TRUST_TEST);
      set_full_list (lv, full_list);
      break;
    case IDC_CHOOSE_FULLPART:
      set_full_list (lv, !full_list);
      break;

    case IDOK:
      if (source == IDC_SOURCE_CWD)
	NEXT (IDD_S_INSTALL);
      else
	NEXT (IDD_S_DOWNLOAD);
      break;

    case IDC_BACK:
      initialized = 0;
      if (source == IDC_SOURCE_CWD)
	NEXT (IDD_ROOT);
      else
	NEXT (IDD_SITE);
      break;

    case IDCANCEL:
      NEXT (0);
      break;
    }
}

static void
GetParentRect (HWND parent, HWND child, RECT *r)
{
  POINT p;
  GetWindowRect (child, r);
  p.x = r->left;
  p.y = r->top;
  ScreenToClient (parent, &p);
  r->left = p.x;
  r->top = p.y;
  p.x = r->right;
  p.y = r->bottom;
  ScreenToClient (parent, &p);
  r->right = p.x;
  r->bottom = p.y;
}

static BOOL CALLBACK
dialog_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  int i, j;
  HWND frame;
  RECT r;
  switch (message)
    {
    case WM_INITDIALOG:
      nextbutton = GetDlgItem (h, IDOK);
      frame = GetDlgItem (h, IDC_LISTVIEW_POS);
      choose_inst_text = GetDlgItem(h,IDC_CHOOSE_INST_TEXT);
      if (source == IDC_SOURCE_DOWNLOAD)
	SetWindowText (choose_inst_text, "Select packages to download ");
      else
	SetWindowText (choose_inst_text, "Select packages to install ");
      GetParentRect (h, frame, &r);
      r.top += 2;
      r.bottom -= 2;
      create_listview (h, &r);
#if 0
      load_dialog (h);
#endif
      return FALSE;
    case WM_COMMAND:
      return HANDLE_WM_COMMAND (h, wParam, lParam, dialog_cmd);
    }
  return FALSE;
}

char *
base (const char *s)
{
  if (!s)
    return 0;
  const char *rv = s;
  while (*s)
    {
      if ((*s == '/' || *s == ':' || *s == '\\') && s[1])
	rv = s + 1;
      s++;
    }
  return (char *) rv;
}

int
find_tar_ext (const char *path)
{
  char *p = strchr (path, '\0') - 7;
  if (p <= path)
    return 0;
  if (*p == '.')
    {
      if (strcmp (p, ".tar.gz") != 0)
	return 0;
    }
  else if (--p <= path || strcmp (p, ".tar.bz2") != 0)
    return 0;

  return p - path;
}

/* Parse a filename into package, version, and extension components. */
int
parse_filename (const char *in_fn, fileparse& f)
{
  char *p, *ver;
  char fn[strlen (in_fn) + 1];

  strcpy (fn, in_fn);
  int n = find_tar_ext (fn);

  if (!n)
    return 0;

  strcpy (f.tail, fn + n);
  fn[n] = '\0';
  f.pkg[0] = f.what[0] = '\0';
  p = base (fn);
  for (ver = p; *ver; ver++)
    if (*ver == '-' || *ver == '_')
      if (isdigit (ver[1]))
	{
	  *ver++ = 0;
	  strcpy (f.pkg, p);
	  break;
	}
      else if (strcasecmp (ver, "-src") == 0 ||
	       strcasecmp (ver, "-patch") == 0)
	{
	  *ver++ = 0;
	  strcpy (f.pkg, p);
	  strcpy (f.what, strlwr (ver));
	  strcpy (f.pkgtar, p);
	  strcat (f.pkgtar, f.tail);
	  ver = strchr (ver, '\0');
	  break;
	}

  if (!f.pkg[0])
    strcpy (f.pkg, p);

  if (!f.what[0])
    {
      p = strchr (ver, '\0');
      strcpy (f.pkgtar, in_fn);
      if ((p -= 4) >= ver && strcasecmp (p, "-src") == 0)
	{
	  strcpy (f.what, "src");
	  *p = '\0';
	  p = f.pkgtar + (p - fn) + 4;
	  memcpy (p - 4, p, strlen (p));
	}
      else if ((p -= 2) >= ver && strcasecmp (p, "-patch") == 0)
	{
	  strcpy (f.what, "patch");
	  *p = '\0';
	  p = f.pkgtar + (p - fn) + 6;
	  memcpy (p - 6, p, strlen (p));
	}
    }

  strcpy (f.ver, *ver ? ver : "0.0");
  return 1;
}

/* Return a pointer to a package given the name. */
Package *
getpkgbyname (const char *pkgname)
{
  for (Package *pkg = package; pkg->name; pkg++)
    if (strcasecmp (pkg->name, pkgname) == 0)
      return pkg;

  return NULL;
}

static void
scan2 (char *path, unsigned int size)
{
  Package *pkg;
  fileparse f;

  if (!parse_filename (path, f))
    return;

  if (f.what[0] != '\0' && f.what[0] != 's')
    return;

  pkg = getpkgbyname (f.pkg);
  if (pkg == NULL)
    return;

  for (Info *inf = pkg->info; inf < pkg->info + NTRUST; inf++)
    if (inf->version && strcmp (f.ver, inf->version) == 0)
      {
	if (f.what[0] == 's')
	  inf->source_exists = -1;
	else
	  inf->install_exists = -1;
	return;
      }

  for (int i = TRUST_CURR; i >= TRUST_PREV; i--)
    if (!pkg->info[i].install)
      {
	Info *inf = pkg->info + i;
	inf->version = strdup (f.ver);
	inf->install = strdup (f.pkgtar);
	if (!inf->source && f.what[0] == 's')
	  {
	    inf->source = strdup (path);
	    inf->source_exists = 1;
	  }
	inf->source_size = size;
	break;
      }
}

static void
scan_downloaded_files ()
{
  find (".", scan2);
}

_Info::_Info (const char *_install, const char *_version, int _install_size,
	    const char *_source, int _source_size)
{
  memset (this, 0, sizeof (*this));
  install = strdup (_install);
  version = strdup (_version);
  install_size = _install_size;
  if (source)
    {
      source = strdup (source);
      source_size = _source_size;
    }
}

static void
read_installed_db ()
{
  int i;
  if (!get_root_dir ())
    return;

  char line[1000], pkgname[1000], inst[1000], src[1000];
  int instsz, srcsz;

  FILE *db = fopen (cygpath ("/etc/setup/installed.db", 0), "rt");
  if (!db)
    return;

  while (fgets (line, 1000, db))
    {
      int parseable;
      src[0] = 0;
      srcsz = 0;
      sscanf (line, "%s %s %d %s %d", pkgname, inst, &instsz, src, &srcsz);

      Package *pkg = getpkgbyname (pkgname);
      fileparse f;
      parseable = parse_filename (inst, f);

      if (pkg == NULL)
	{
	  if (!parseable)
	    continue;
	  pkg = new_package (pkgname);
	  pkg->info[TRUST_CURR].version = strdup (f.ver);
	  pkg->info[TRUST_CURR].install = strdup (inst);
	  pkg->info[TRUST_CURR].install_size = instsz;
	  if (src && srcsz)
	    {
	      pkg->info[TRUST_CURR].source = strdup (src);
	      pkg->info[TRUST_CURR].source_size = srcsz;
	    }
	  pkg->installed_ix = TRUST_CURR;
	}

      pkg->installed = new Info (inst, f.ver, instsz);

      if (!pkg->installed_ix)
	for (trusts t = TRUST_PREV; t < NTRUST; ((int) t)++)
	  if (pkg->info[t].install && strcmp (f.ver, pkg->info[t].version) == 0)
	    {
	      pkg->installed_ix = t;
	      break;
	    }
    }
  fclose (db);
}

int
package_sort (const void *va, const void *vb)
{
  Package *a = (Package *)va;
  Package *b = (Package *)vb;
  return strcmp (a->name, b->name);
}

void
do_choose (HINSTANCE h)
{
  int rv;

  qsort (package, npackages, sizeof (package[0]), package_sort);

  nextbutton = 0;
  bm_spin = LoadImage (h, MAKEINTRESOURCE (IDB_SPIN), IMAGE_BITMAP, 0, 0, 0);
  bm_rtarrow = LoadImage (h, MAKEINTRESOURCE (IDB_RTARROW), IMAGE_BITMAP, 0, 0, 0);

  bm_checkyes = LoadImage (h, MAKEINTRESOURCE (IDB_CHECK_YES), IMAGE_BITMAP, 0, 0, 0);
  bm_checkno = LoadImage (h, MAKEINTRESOURCE (IDB_CHECK_NO), IMAGE_BITMAP, 0, 0, 0);
  bm_checkna = LoadImage (h, MAKEINTRESOURCE (IDB_CHECK_NA), IMAGE_BITMAP, 0, 0, 0);

  register_windows (h);

  if (source == IDC_SOURCE_DOWNLOAD || source == IDC_SOURCE_CWD)
    scan_downloaded_files ();

  read_installed_db ();
  set_existence ();

  rv = DialogBox (h, MAKEINTRESOURCE (IDD_CHOOSE), 0, dialog_proc);
  if (rv == -1)
    fatal (IDS_DIALOG_FAILED);

  log (LOG_BABBLE, "Chooser results...");
  for (Package *pkg = package; pkg->name; pkg++)
    {
      static char *infos[] = {"prev", "curr", "test"};
      const char *trust = ((pkg->trust == TRUST_PREV) ? "prev"
			   : (pkg->trust == TRUST_CURR) ? "curr"
			   : (pkg->trust == TRUST_TEST) ? "test"
			   : "unknown");
      const char *action = choose_caption (pkg);
      const char *installed = ((pkg->installed_ix == -1) ? "none"
			       : (pkg->installed_ix == TRUST_PREV) ? "prev"
			       : (pkg->installed_ix == TRUST_CURR) ? "curr"
			       : (pkg->installed_ix == TRUST_TEST) ? "test"
			       : "unknown");
      const char *excluded = (pkg->exclude ? "yes" : "no");

      log (LOG_BABBLE, "[%s] action=%s trust=%s installed=%s excluded=%s src?=%s",
	   pkg->name, action, trust, installed,
	   excluded, pkg->srcpicked ? "yes" : "no");
      for (int t = 1; t < NTRUST; t++)
	{
	  if (pkg->info[t].install)
	    log (LOG_BABBLE, "     [%s] ver=%s\r\n"
			     "          inst=%s %d exists=%s\r\n"
			     "		src=%s %d exists=%s",
		 infos[t],
		 pkg->info[t].version ?: "(none)",
		 pkg->info[t].install ?: "(none)",
		 pkg->info[t].install_size,
		 (pkg->info[t].install_exists == 1) ? "yes":"no",
		 pkg->info[t].source ?: "(none)",
		 pkg->info[t].source_size,
		 (pkg->info[t].source_exists == 1) ? "yes":"no");
	}
    }
}
