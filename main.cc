/*
 * Copyright (c) 2000, Red Hat, Inc.
 * Copyright (c) 2003, Robert Collins <rbtcollins@hotmail.com>
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
 *            Robert Collins <rbtcollins@hotmail.com>
 *
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

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
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
#include "LogFile.h"
#include "setup_version.h"

#include "proppage.h"
#include "propsheet.h"

// Page class headers
#include "splash.h"
#include "AntiVirus.h"
#include "source.h"
#include "root.h"
#include "localdir.h"
#include "net.h"
#include "site.h"
#include "choose.h"
#include "prereq.h"
#include "threebar.h"
#include "desktop.h"

#include "getopt++/GetOption.h"
#include "getopt++/BoolOption.h"

#include "UserSettings.h"
#include "Exception.h"
#include <stdexcept>

#include <wincon.h>
#include <fstream>

using namespace std;

HINSTANCE hinstance;

static BoolOption UnattendedOption (false, 'q', "quiet-mode", "Unattended setup mode");
static BoolOption HelpOption (false, 'h', "help", "print help");

static void inline
set_cout ()
{
  HMODULE hm = LoadLibrary ("kernel32.dll");
  if (!hm)
    return;

  BOOL WINAPI (*dyn_AttachConsole) (DWORD) = (BOOL WINAPI (*)(DWORD)) GetProcAddress (hm, "AttachConsole");
  if (dyn_AttachConsole)
    {
      HANDLE hstdout = GetStdHandle (STD_OUTPUT_HANDLE);
      if (GetFileType (hstdout) == FILE_TYPE_UNKNOWN && GetLastError () != NO_ERROR
	  && dyn_AttachConsole ((DWORD) -1))
	  {
	    ofstream *conout = new ofstream ("conout$");
	    cout.rdbuf (conout->rdbuf ());
	    cout.flush ();
	  }
    }
  FreeLibrary (hm);
}

// Other threads talk to this page, so we need to have it externable.
ThreeBarProgressPage Progress;

// This is a little ugly, but the decision about where to log occurs
// after the source is set AND the root mount obtained
// so we make the actual logger available to the appropriate routine(s).
LogFile *theLog;

#ifndef __CYGWIN__
int WINAPI
WinMain (HINSTANCE h,
	 HINSTANCE hPrevInstance, LPSTR command_line, int cmd_show)
{

  hinstance = h;
#else
int
main (int argc, char **argv)
{
  hinstance = GetModuleHandle (NULL);
#endif

  try {
    char cwd[MAX_PATH];
    GetCurrentDirectory (MAX_PATH, cwd);
    local_dir = std::string (cwd);

    // TODO: make an equivalent for __argv under cygwin.
    char **_argv;
#ifndef __CYGWIN__
    int argc;
    for (argc = 0, _argv = __argv; *_argv; _argv++)
      ++argc;
    _argv = __argv;
#else
    _argv = argv;
#endif

    if (!GetOption::GetInstance ().Process (argc,_argv, NULL))
      exit (1);

    unattended_mode = UnattendedOption;
    if (unattended_mode || HelpOption)
      set_cout ();

    LogSingleton::SetInstance (*(theLog = LogFile::createLogFile ()));
    theLog->setFile (LOG_BABBLE, local_dir + "/setup.log.full", false);
    theLog->setFile (0, local_dir + "/setup.log", true);

    log (LOG_PLAIN) << "Starting cygwin install, version " 
                    << setup_version << endLog;

    UserSettings::Instance ().loadAllSettings ();

    SplashPage Splash;
    AntiVirusPage AntiVirus;
    SourcePage Source;
    RootPage Root;
    LocalDirPage LocalDir;
    NetPage Net;
    SitePage Site;
    ChooserPage Chooser (cmd_show);
    PrereqPage Prereq;
    DesktopSetupPage Desktop;
    PropSheet MainWindow;

    log (LOG_TIMESTAMP) << "Current Directory: " << local_dir << endLog;

    if (HelpOption)
    {
      GetOption::GetInstance ().ParameterUsage (log (LOG_PLAIN)
                                                << "\nCommand Line Options:\n");
      theLog->exit (0);
    }

    /* Set the default DACL and Group only on NT/W2K. 9x/ME has 
       no idea of access control lists and security at all.  */
    if (IsWindowsNT ())
      nt_sec.setDefaultSecurity ();

    // Initialize common controls
    InitCommonControls ();

    // Init window class lib
    Window::SetAppInstance (hinstance);

    // Create pages
    Splash.Create ();
    AntiVirus.Create ();
    Source.Create ();
    Root.Create ();
    LocalDir.Create ();
    Net.Create ();
    Site.Create ();
    Chooser.Create ();
    Prereq.Create ();
    Progress.Create ();
    Desktop.Create ();

    // Add pages to sheet
    MainWindow.AddPage (&Splash);
    MainWindow.AddPage (&AntiVirus);
    MainWindow.AddPage (&Source);
    MainWindow.AddPage (&Root);
    MainWindow.AddPage (&LocalDir);
    MainWindow.AddPage (&Net);
    MainWindow.AddPage (&Site);
    MainWindow.AddPage (&Chooser);
    MainWindow.AddPage (&Prereq);
    MainWindow.AddPage (&Progress);
    MainWindow.AddPage (&Desktop);

    // Create the PropSheet main window
    MainWindow.Create ();

    // Clean exit.. save user options.
    UserSettings::Instance().saveAllSettings();
    if (rebootneeded)
      {
	theLog->exit (IDS_REBOOT_REQUIRED);
      }
    else
      {
	theLog->exit (0);
      }
  }
  TOPLEVEL_CATCH("main");

  // Never reached
  return 0;
}
