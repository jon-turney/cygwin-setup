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

/* The purpose of this file is to ask the user where they want the
   root of the installation to be, and to ask whether the user prefers
   text or binary mounts. */

#include "root.h"

#include "LogSingleton.h"

#include "win32.h"
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "ini.h"
#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "msg.h"
#include "package_db.h"
#include "mount.h"
#include "propsheet.h"

#include "getopt++/StringOption.h"

using namespace std;

StringOption RootOption ("", 'R', "root", "Root installation directory", false);

static ControlAdjuster::ControlInfo RootControlsInfo[] = {
  { IDC_ROOTDIR_GRP,              CP_STRETCH,           CP_TOP      },
  { IDC_ROOT_DIR,                 CP_STRETCH,           CP_TOP      },
  { IDC_ROOT_BROWSE,              CP_RIGHT,             CP_TOP      },

  { IDC_INSTALLFOR_GRP,           CP_STRETCH,		CP_STRETCH  },
  { IDC_ROOT_SYSTEM,              CP_LEFT,              CP_TOP      },
  { IDC_ALLUSERS_TEXT,            CP_STRETCH,		CP_TOP      },
  { IDC_ROOT_USER,                CP_LEFT,              CP_BOTTOM   },
  { IDC_JUSTME_TEXT,              CP_STRETCH,		CP_BOTTOM   },

  {0, CP_LEFT, CP_TOP}
};

static int su[] = { IDC_ROOT_SYSTEM, IDC_ROOT_USER, 0 };

static string orig_root_dir;

void
RootPage::check_if_enable_next (HWND h)
{
  DWORD ButtonFlags = PSWIZB_BACK;
  // if there's something in the root dir box, and we have a scope, enable next
  if (egetString (h, IDC_ROOT_DIR).size() && root_scope)
    ButtonFlags |= PSWIZB_NEXT;
  GetOwner ()->SetButtons (ButtonFlags);
}

static void
load_dialog (HWND h)
{
  rbset (h, su, root_scope);
  eset (h, IDC_ROOT_DIR, get_root_dir ());
}

static void
save_dialog (HWND h)
{
  root_scope = rbget (h, su);
  set_root_dir (egetString (h, IDC_ROOT_DIR));
}

static int CALLBACK
browse_cb (HWND h, UINT msg, LPARAM lp, LPARAM data)
{
  switch (msg)
    {
    case BFFM_INITIALIZED:
      if (get_root_dir ().size())
	SendMessage (h, BFFM_SETSELECTION, TRUE, (LPARAM) get_root_dir ().c_str());
      break;
    }
  return 0;
}

static void
browse (HWND h)
{
  BROWSEINFO bi;
  /* SHGetPathFromIDList doesn't handle path length > MAX_PATH. */
  CHAR name[MAX_PATH];
  LPITEMIDLIST pidl;
  memset (&bi, 0, sizeof (bi));
  bi.hwndOwner = h;
  bi.pszDisplayName = name;
  bi.lpszTitle = "Select an installation root directory";
  bi.ulFlags = BIF_RETURNONLYFSDIRS;
  bi.lpfn = browse_cb;
  pidl = SHBrowseForFolder (&bi);
  if (pidl)
    {
      if (SHGetPathFromIDList (pidl, name))
	eset (h, IDC_ROOT_DIR, name);
    }
}

static int
directory_is_absolute ()
{
  const std::string &r = get_root_dir ();
  if (isalpha (r[0]) && r[1] == ':' && isdirsep (r[2]))
    return 1;
  return 0;
}

static int
directory_is_rootdir ()
{
  const std::string &r = get_root_dir ();
  size_t pos = r.find_first_of ("/\\");
  if (pos != std::string::npos)
    {
      while (isdirsep (r[++pos]))
	;
      if (r[pos])
	return 0;
    }
  return 1;
}

static int
directory_has_spaces ()
{
  if (std::string(get_root_dir()).find(' ') != std::string::npos)
    return 1;
  return 0;
}

static int
directory_contains_wrong_version (HWND h)
{
  HANDLE fh;
  char text[512];
  std::string cygwin_dll = get_root_dir() + "\\bin\\cygwin1.dll";

  /* Check if we have a cygwin1.dll.  If not, this is a new install.
     If yes, check if the target machine type of this setup version is the
     same as the machine type of the install Cygwin DLL.  If yes, just go
     ahead.  If not, show a message and indicate this to the caller.

     If anything goes wrong reading the header of cygwin1.dll, we check
     cygcheck.exe's binary type.  This also covers the situation that the
     installed cygwin1.dll is broken for some reason. */
  fh = CreateFileA (cygwin_dll.c_str (), GENERIC_READ, FILE_SHARE_VALID_FLAGS,
		    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (fh != INVALID_HANDLE_VALUE)
    {
      DWORD read = 0;
      struct {
	LONG dos_header[32];
	IMAGE_NT_HEADERS32 nt_header;
      } hdr;

      ReadFile (fh, &hdr, sizeof hdr, &read, NULL);
      CloseHandle (fh);
      if (read != sizeof hdr)
	fh = INVALID_HANDLE_VALUE;
      else
	{
	  /* 32 bit setup and 32 bit inst? */
	  if (hdr.nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_I386
	      && !is_64bit)
	    return 0;
	  /* 64 bit setup and 64 bit inst? */
	  if (hdr.nt_header.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64
	      && is_64bit)
	    return 0;
	}
    }
  if (fh == INVALID_HANDLE_VALUE)
    {
      DWORD type;
      std::string cygcheck_exe = get_root_dir() + "\\bin\\cygcheck.exe";

      /* Probably new installation */
      if (!GetBinaryType (cygcheck_exe.c_str (), &type))
        {
          is_new_install = true;
          return 0;
        }
      /* 64 bit setup and 64 bit inst? */
      if (type == SCS_32BIT_BINARY && !is_64bit)
	return 0;
      /* 32 bit setup and 32 bit inst? */
      if (type == SCS_64BIT_BINARY && is_64bit)
	return 0;
    }

  /* Forestall mixing. */
  const char *setup_ver = is_64bit ? "64" : "32";
  const char *inst_ver = is_64bit ? "32" : "64";
  snprintf (text, sizeof text,
	"You're trying to install a %s bit version of Cygwin into a directory\n"
	"containing a %s bit version of Cygwin.  Continuing to do so would\n"
	"break the existing installation.\n\n"
	"Either run http://cygwin.com/setup-%s.exe to update your\n"
	"existing %s bit installation of Cygwin, or choose another directory\n"
	"for your %s bit installation.",
	setup_ver, inst_ver,
	is_64bit ? "x86" : "x86_64",
	inst_ver, setup_ver);
  mbox (h, text, "Target CPU mismatch", MB_OK);
  return 1;
}

bool
RootPage::OnMessageCmd (int id, HWND hwndctl, UINT code)
{
  switch (id)
    {

    case IDC_ROOT_DIR:
    case IDC_ROOT_SYSTEM:
    case IDC_ROOT_USER:
      check_if_enable_next (GetHWND ());
      break;

    case IDC_ROOT_BROWSE:
      browse (GetHWND ());
      break;
    default:
      return false;
    }
  return true;
}

RootPage::RootPage ()
{
  sizeProcessor.AddControlInfo (RootControlsInfo);
}

bool
RootPage::Create ()
{
  return PropertyPage::Create (IDD_ROOT);
}

void
RootPage::OnInit ()
{
  if (((string)RootOption).size()) 
    set_root_dir((string)RootOption);
  if (!get_root_dir ().size())
    read_mounts (std::string ());
  orig_root_dir = get_root_dir();
  load_dialog (GetHWND ());
}

void
RootPage::OnActivate ()
{
  check_if_enable_next (GetHWND ());
}

bool
RootPage::wantsActivation() const
{
  return (source != IDC_SOURCE_DOWNLOAD);
}

long
RootPage::OnNext ()
{
  HWND h = GetHWND ();

  save_dialog (h);

  if (!directory_is_absolute ())
    {
      note (h, IDS_ROOT_ABSOLUTE);
      return -1;
    }
  else if (get_root_dir() != orig_root_dir &&
           directory_is_rootdir () && (IDNO == yesno (h, IDS_ROOT_SLASH)))
    return -1;
  else if (directory_has_spaces () && (IDNO == yesno (h, IDS_ROOT_SPACE)))
    return -1;
  else if (directory_contains_wrong_version (h))
    return -1;

  Log (LOG_PLAIN) << "root: " << get_root_dir ()
    << (root_scope == IDC_ROOT_USER ? " user" : " system") << endLog;

  return 0;
}

long
RootPage::OnBack ()
{
  HWND h = GetHWND ();

  save_dialog (h);
  return 0;
}

long
RootPage::OnUnattended ()
{
  return OnNext();
}
