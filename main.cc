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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <commctrl.h>

#include <stdio.h>
#include <stdlib.h>
#include "resource.h"
#include "dialog.h"
#include "state.h"
#include "msg.h"
#include "find.h"
#include "mount.h"
#include "log.h"
#include "version.h"

#include "port.h"
#include "proppage.h"
#include "propsheet.h"

// Page class headers
#include "splash.h"
#include "source.h"
#include "root.h"
#include "localdir.h"
#include "net.h"
#include "site.h"
#include "choose.h"
#include "threebar.h"
#include "desktop.h"

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
  SID_IDENTIFIER_AUTHORITY sid_auth = { SECURITY_WORLD_SID_AUTHORITY };
  if (!AllocateAndInitializeSid (&sid_auth, 1, 0, 0, 0, 0, 0, 0, 0, 0, &sid))
    {
      log (LOG_TIMESTAMP, "AllocateAndInitializeSid() failed: %lu",
	   GetLastError ());
      return;
    }

  /* Create the ACE which grants full access to "Everyone" and store it
     in dacl->DefaultDacl. */
  if (!AddAccessAllowedAce
      (dacl->DefaultDacl, ACL_REVISION, GENERIC_ALL, sid))
    {
      log (LOG_TIMESTAMP, "AddAccessAllowedAce() failed: %lu",
	   GetLastError ());
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

// Other threads talk to this page, so we need to have it externable.
ThreeBarProgressPage Progress;

int WINAPI
WinMain (HINSTANCE h,
	 HINSTANCE hPrevInstance, LPSTR command_line, int cmd_show)
{
  hinstance = h;

  next_dialog = IDD_SPLASH;

  log (LOG_TIMESTAMP, "Starting cygwin install, version %s", version);

  SplashPage Splash;
  SourcePage Source;
  RootPage Root;
  LocalDirPage LocalDir;
  NetPage Net;
  SitePage Site;
  ChooserPage Chooser;
  DesktopSetupPage Desktop;
  PropSheet MainWindow;

  char cwd[_MAX_PATH];
  GetCurrentDirectory (sizeof (cwd), cwd);
  local_dir = new char [strlen (cwd) + 1];
  local_dir [strlen (cwd)] = '\0';
  strcpy (local_dir, cwd);
  log (0, "Current Directory: %s", cwd);

  char **argv;
  int argc;
  log (LOG_TIMESTAMP, "Command line parameters\n");
  for (argc = 0, argv = __argv; *argv; argv++)
    log (LOG_TIMESTAMP, "%d - '%s'\n", argc++, *argv);
  log (LOG_TIMESTAMP, "%d parameters passed\n", argc);

  /* Set the default DACL only on NT/W2K. 9x/ME has no idea of access
     control lists and security at all. */
  if (iswinnt)
    set_default_dacl ();

  // Initialize common controls
  InitCommonControls ();

  // Init window class lib
  Window::SetAppInstance (h);

  // Create pages
  Splash.Create ();
  Source.Create ();
  Root.Create ();
  LocalDir.Create ();
  Net.Create ();
  Site.Create ();
  Chooser.Create ();
  Progress.Create ();
  Desktop.Create ();

  // Add pages to sheet
  MainWindow.AddPage (&Splash);
  MainWindow.AddPage (&Source);
  MainWindow.AddPage (&Root);
  MainWindow.AddPage (&LocalDir);
  MainWindow.AddPage (&Net);
  MainWindow.AddPage (&Site);
  MainWindow.AddPage (&Chooser);
  MainWindow.AddPage (&Progress);
  MainWindow.AddPage (&Desktop);

  // Create the PropSheet main window
  MainWindow.Create ();

  exit_setup (0);
  /* Keep gcc happy :} */
  return 0;
}
