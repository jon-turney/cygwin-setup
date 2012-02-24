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
#define CINTERFACE
#include "win32.h"
#include <commctrl.h>
#include "shlobj.h"

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
#include "postinstallresults.h"

#include "getopt++/GetOption.h"
#include "getopt++/BoolOption.h"

#include "Exception.h"
#include <stdexcept>

#include "UserSettings.h"
#include "SourceSetting.h"
#include "ConnectionSetting.h"
#include "KeysSetting.h"

#include <wincon.h>
#include <fstream>

using namespace std;

HINSTANCE hinstance;
bool is_legacy;

static BoolOption UnattendedOption (false, 'q', "quiet-mode", "Unattended setup mode");
static BoolOption PackageManagerOption (false, 'M', "package-manager", "Semi-attended chooser-only mode");
static BoolOption HelpOption (false, 'h', "help", "print help");
static BOOL WINAPI (*dyn_AttachConsole) (DWORD);
static BOOL WINAPI (*dyn_GetLongPathName) (LPCTSTR, LPTSTR, DWORD);


static void inline
set_dynaddr ()
{
  HMODULE hm = LoadLibrary ("kernel32.dll");
  if (!hm)
    return;

  dyn_AttachConsole = (BOOL WINAPI (*)(DWORD)) GetProcAddress (hm, "AttachConsole");
  dyn_GetLongPathName = (BOOL WINAPI (*)(LPCTSTR, LPTSTR, DWORD)) GetProcAddress (hm, "GetLongPathNameA");
}

static void inline
set_cout ()
{
  HANDLE my_stdout = GetStdHandle (STD_OUTPUT_HANDLE);
  if (my_stdout != INVALID_HANDLE_VALUE && GetFileType (my_stdout) != FILE_TYPE_UNKNOWN)
    return;

  if (dyn_AttachConsole && dyn_AttachConsole ((DWORD) -1))
    {
      ofstream *conout = new ofstream ("conout$");
      cout.rdbuf (conout->rdbuf ());
      cout.flush ();
    }
}

// Other threads talk to these pages, so we need to have it externable.
ThreeBarProgressPage Progress;
PostInstallResultsPage PostInstallResults;

// This is a little ugly, but the decision about where to log occurs
// after the source is set AND the root mount obtained
// so we make the actual logger available to the appropriate routine(s).
LogFile *theLog;

static inline void
main_display ()
{
  /* nondisplay classes */
  LocalDirSetting localDir;
  SourceSetting SourceSettings;
  ConnectionSetting ConnectionSettings;
  SiteSetting ChosenSites;
  ExtraKeysSetting ExtraKeys;

  SplashPage Splash;
  AntiVirusPage AntiVirus;
  SourcePage Source;
  RootPage Root;
  LocalDirPage LocalDir;
  NetPage Net;
  SitePage Site;
  ChooserPage Chooser;
  PrereqPage Prereq;
  DesktopSetupPage Desktop;
  PropSheet MainWindow;

  log (LOG_TIMESTAMP) << "Current Directory: " << local_dir << endLog;

  /* Set the default DACL and Group only on NT/W2K. 9x/ME has 
     no idea of access control lists and security at all.  */
  if (IsWindowsNT ())
    nt_sec.setDefaultSecurity ();

  // Initialize common controls
  INITCOMMONCONTROLSEX icce = { sizeof (INITCOMMONCONTROLSEX),
				ICC_WIN95_CLASSES };
  InitCommonControlsEx (&icce);

  // Initialize COM and ShellLink instance here.  For some reason
  // Windows 7 fails to create the ShellLink instance if this is
  // done later, in the thread which actually creates the shortcuts.
  extern IShellLink *sl;
  CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
  HRESULT res = CoCreateInstance (&CLSID_ShellLink, NULL,
				  CLSCTX_INPROC_SERVER, &IID_IShellLink,
				  (LPVOID *) & sl);
  if (res)
    {
      char buf[256];
      sprintf (buf, "CoCreateInstance failed with error %p.\n"
		    "Setup will not be able to create Cygwin Icons\n"
		    "in the Start Menu or on the Desktop.", (void *) res);
      MessageBox (NULL, buf, "Cygwin Setup", MB_OK);
    }

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
  PostInstallResults.Create ();
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
  MainWindow.AddPage (&PostInstallResults);
  MainWindow.AddPage (&Desktop);

  // Create the PropSheet main window
  MainWindow.Create ();

  // Uninitalize COM
  if (sl)
    sl->lpVtbl->Release (sl);
  CoUninitialize ();
}

static void
set_legacy (const char *command)
{
  char buf[MAX_PATH + 1];
  if (strchr (command, '~') == NULL || !dyn_GetLongPathName
      || !dyn_GetLongPathName (command, buf, MAX_PATH))
    {
      strncpy (buf, command, MAX_PATH);
      buf[MAX_PATH] = '\0';
    }
  strlwr (buf);
  is_legacy = strstr (buf, "setup-legacy");
}

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

  set_dynaddr ();
  // Make sure the C runtime functions use the same codepage as the GUI
  char locale[12];
  snprintf(locale, sizeof locale, ".%u", GetACP());
  setlocale(LC_ALL, locale);

  set_legacy (_argv[0]);

  if (is_legacy && IsWindowsNT ())
    {
      if (MessageBox (NULL,
		      "You are attempting to install a legacy version of Cygwin\n"
		      "on a modern version of Windows.  Press \"OK\" if this is\n"
		      "really want you want to do.  Otherwise press \"Cancel\".\n"
		      "See http://cygwin.com/ for more information.",
		      "Attempt to install legacy version of Cygwin",
		      MB_OKCANCEL | MB_ICONSTOP | MB_SETFOREGROUND | MB_TOPMOST)
	  == IDCANCEL)
	return 1;
    }
  else if (!is_legacy && !IsWindowsNT ())
    {
      MessageBox (NULL,
		  "Cygwin 1.7 and later does not run on Windows 95,\n"
		  "Windows 98, or Windows Me.  If you want to install Cygwin\n"
		  "on one of these systems, please install an older version.\n"
		  "See http://cygwin.com/ for more information.",
		  "Unsupported version of Windows detected",
		  MB_OK | MB_ICONSTOP | MB_SETFOREGROUND | MB_TOPMOST);
      return 1;
    }

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

    unattended_mode = PackageManagerOption ? chooseronly
			: (UnattendedOption ? unattended : attended);

    if (unattended_mode || HelpOption)
      set_cout ();

    LogSingleton::SetInstance (*(theLog = LogFile::createLogFile ()));
    const char *sep = isdirsep (local_dir[local_dir.size () - 1]) ? "" : "\\";
    theLog->setFile (LOG_BABBLE, local_dir + sep + "setup.log.full", false);
    theLog->setFile (0, local_dir + sep + "setup.log", true);

    log (LOG_PLAIN) << "Starting cygwin install, version " 
                    << setup_version << endLog;

    if (HelpOption)
      {
        GetOption::GetInstance ().ParameterUsage (log (LOG_PLAIN)
                                                  << "\nCommand Line Options:\n");
      }
    else
      {
        UserSettings Settings (local_dir);

        main_display ();

        Settings.save ();	// Clean exit.. save user options.
      }

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
