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

/* OK, here's how this works.  Each of the steps needed for install -
   dialogs, downloads, installs - are in their own files and have some
   "do_*" function (prototype in dialog.h) and a resource id (IDD_* or
   IDD_S_* in resource.h) for that step.  Each step is responsible for
   selecting the next step!  See the NEXT macro in dialog.h.  Note
   that the IDD_S_* ids are fake; those are for steps that don't
   really have a controlling dialog (some have progress dialogs, but
   those don't count, although they could).  Replace the IDD_S_* with
   IDD_* if you create a real dialog for those steps. */

static char *cvsid = "\n%%% $Id$\n";

#include "win32.h"

#include <stdio.h>
#include <stdlib.h>
#include "resource.h"
#include "dialog.h"
#include "state.h"
#include "msg.h"
#include "netio.h"
#include "find.h"
#include "mount.h"
#include "log.h"
#include "version.h"

#include "port.h"

void netio_test (char *);

int next_dialog;
int exit_msg = 0;

HINSTANCE hinstance;

/* Maximum size of a SID on NT/W2K. */
#define MAX_SID_LEN	40

/* Computes the size of an ACL in relation to the number of ACEs it
   should contain. */
#define TOKEN_ACL_SIZE(cnt) (sizeof(ACL) + \
			     (cnt) * (sizeof(ACCESS_ALLOWED_ACE) + MAX_SID_LEN))

#define iswinnt		(GetVersion() < 0x80000000)

void
set_default_dacl ()
{
  /* To assure that the created files have a useful ACL, the 
  default DACL in the process token is set to full access to
  everyone. This applies to files and subdirectories created
  in directories which don't propagate permissions to child
  objects. */

  /* Create a buffer which has enough room to contain the TOKEN_DEFAULT_DACL
     structure plus an ACL with one ACE. */
  char buf[sizeof (TOKEN_DEFAULT_DACL) + TOKEN_ACL_SIZE (1)];

  /* First initialize the TOKEN_DEFAULT_DACL structure. */
  PTOKEN_DEFAULT_DACL dacl = (PTOKEN_DEFAULT_DACL) buf;
  dacl->DefaultDacl = (PACL) (buf + sizeof *dacl);

  /* Initialize the ACL for containing one ACE. */
  if (!InitializeAcl (dacl->DefaultDacl, TOKEN_ACL_SIZE (1), ACL_REVISION))
    {
      log (LOG_TIMESTAMP, "InitializeAcl() failed: %lu", GetLastError ());
      return;
    }

  /* Get the SID for "Everyone". */
  PSID sid;
  SID_IDENTIFIER_AUTHORITY sid_auth = SECURITY_WORLD_SID_AUTHORITY;
  if (!AllocateAndInitializeSid(&sid_auth, 1, 0, 0, 0, 0, 0, 0, 0, 0, &sid))
    {
      log (LOG_TIMESTAMP, "AllocateAndInitializeSid() failed: %lu",
	   GetLastError ());
      return;
    }

  /* Create the ACE which grants full access to "Everyone" and store it
     in dacl->DefaultDacl. */
  if (!AddAccessAllowedAce (dacl->DefaultDacl, ACL_REVISION, GENERIC_ALL, sid))
    {
      log (LOG_TIMESTAMP, "AddAccessAllowedAce() failed: %lu", GetLastError ());
      goto out;
    }

  /* Get the processes access token. */
  HANDLE token;
  if (!OpenProcessToken (GetCurrentProcess (),
  			 TOKEN_READ | TOKEN_ADJUST_DEFAULT, &token))
    {
      log (LOG_TIMESTAMP, "OpenProcessToken() failed: %lu", GetLastError ());
      goto out;
    }

  /* Set the default DACL to the above computed ACL. */
  if (!SetTokenInformation (token, TokenDefaultDacl, dacl, sizeof buf))
    log (LOG_TIMESTAMP, "OpenProcessToken() failed: %lu", GetLastError ());

  /* Close token handle. */
  CloseHandle (token);

out:
  /* Free memory occupied by the "Everyone" SID. */
  FreeSid (sid);
}

int WINAPI
WinMain (HINSTANCE h,
	 HINSTANCE hPrevInstance,
	 LPSTR command_line,
	 int cmd_show)
{
  hinstance = h;

  next_dialog = IDD_SPLASH;

  log (LOG_TIMESTAMP, "Starting cygwin install, version %s", version);

  char cwd[_MAX_PATH];
  GetCurrentDirectory (sizeof (cwd), cwd);
  local_dir = strdup (cwd);
  log (0, "Current Directory: %s", cwd);

  /* Set the default DACL only on NT/W2K. 9x/ME has no idea of access
     control lists and security at all. */
  if (iswinnt)
    set_default_dacl ();

  while (next_dialog)
    {
      switch (next_dialog)
	{
	case IDD_SPLASH:	do_splash (h);	break;
	case IDD_SOURCE:	do_source (h);	break;
	case IDD_LOCAL_DIR:	do_local_dir (h); break;
	case IDD_ROOT:		do_root (h);	break;
	case IDD_NET:		do_net (h);	break;
	case IDD_SITE:		do_site (h);	break;
	case IDD_OTHER_URL:	do_other (h);	break;
	case IDD_S_LOAD_INI:	do_ini (h);	break;
	case IDD_S_FROM_CWD:	do_fromcwd (h);	break;
	case IDD_CHOOSE:	do_choose (h);	break;
	case IDD_S_DOWNLOAD:	do_download (h); break;
	case IDD_S_INSTALL:	do_install (h);	break;
	case IDD_DESKTOP:	do_desktop (h); break;
	case IDD_S_POSTINSTALL:	do_postinstall (h); break;

	default:
	  next_dialog = 0;
	  break;
	}
    }

  exit_setup (0);
}
