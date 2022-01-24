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
 *	      Robert Collins <rbtcollins@hotmail.com>
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

#include "win32.h"
#include <commctrl.h>
#include <shellapi.h>
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
#include "confirm.h"
#include "threebar.h"
#include "desktop.h"
#include "postinstallresults.h"

#include "getopt++/GetOption.h"
#include "getopt++/BoolOption.h"
#include "getopt++/StringOption.h"
#include "getopt++/StringChoiceOption.h"
#include "mklink2.h"

#include "Exception.h"
#include <stdexcept>

#include "UserSettings.h"
#include "SourceSetting.h"
#include "ConnectionSetting.h"
#include "KeysSetting.h"

#include <wincon.h>
#include <fstream>

#ifdef __MINGW64_VERSION_MAJOR
extern char **_argv;
#endif

bool is_64bit;
bool is_new_install = false;
std::string SetupArch;
std::string SetupIniDir;

HINSTANCE hinstance;

static StringChoiceOption::StringChoices symlink_types({
    {"native", SymlinkTypeNative},
    {"lnk", SymlinkTypeShortcut},
    {"sys", SymlinkTypeMagic},
    {"wsl", SymlinkTypeWsl},
  });

static StringOption Arch ("", 'a', "arch", "Architecture to install (x86_64 or x86)", false);
static BoolOption UnattendedOption (false, 'q', "quiet-mode", "Unattended setup mode");
static BoolOption PackageManagerOption (false, 'M', "package-manager", "Semi-attended chooser-only mode");
static BoolOption NoAdminOption (false, 'B', "no-admin", "Do not check for and enforce running as Administrator");
static BoolOption WaitOption (false, 'W', "wait", "When elevating, wait for elevated child process");
static BoolOption HelpOption (false, 'h', "help", "Print help");
static BoolOption VersionOption (false, 'V', "version", "Show version");
static StringOption SetupBaseNameOpt ("setup", 'i', "ini-basename", "Use a different basename, e.g. \"foo\", instead of \"setup\"", false);
BoolOption UnsupportedOption (false, '\0', "allow-unsupported-windows", "Allow old, unsupported Windows versions");
static BoolOption DeprecatedOption (false, 'w', "no-warn-deprecated-windows", "Warn about deprecated Windows versions");
static StringChoiceOption SymlinkTypeOption(symlink_types, '\0', "symlink-type", "Symlink type (lnk, native, sys, wsl)", false, SymlinkTypeMagic);
static StringOption GuiLangOption ("", '\0', "lang", "Specify GUI language langid");

std::string SetupBaseName;

static void inline
set_cout ()
{
  HANDLE my_stdout = GetStdHandle (STD_OUTPUT_HANDLE);
  if (my_stdout != INVALID_HANDLE_VALUE && GetFileType (my_stdout) != FILE_TYPE_UNKNOWN)
    return;

  if (AttachConsole ((DWORD) -1))
    {
      std::ofstream *conout = new std::ofstream ("conout$");
      std::cout.rdbuf (conout->rdbuf ());
      std::cout.flush ();
    }
}

// Other threads talk to these pages, so we need to have it externable.
ThreeBarProgressPage Progress;
PostInstallResultsPage PostInstallResults;

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
  ConfirmPage Confirm;
  DesktopSetupPage Desktop;
  PropSheet MainWindow;

  Log (LOG_TIMESTAMP) << "Current Directory: " << local_dir << endLog;

  // Initialize common controls
  INITCOMMONCONTROLSEX icce = { sizeof (INITCOMMONCONTROLSEX),
				ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES };
  InitCommonControlsEx (&icce);

  // Initialize COM and ShellLink instance here.  For some reason
  // Windows 7 fails to create the ShellLink instance if this is
  // done later, in the thread which actually creates the shortcuts.
  extern IShellLink *sl;
  CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
  HRESULT res = CoCreateInstance (CLSID_ShellLink, NULL,
				  CLSCTX_INPROC_SERVER, IID_IShellLink,
				  (LPVOID *) & sl);
  if (res)
    {
      mbox (NULL, IDS_SHELLLINK_FAILED, MB_OK, res);
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
  Confirm.Create ();
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
  MainWindow.AddPage (&Confirm);
  MainWindow.AddPage (&Progress);
  MainWindow.AddPage (&PostInstallResults);
  MainWindow.AddPage (&Desktop);

  // Create the PropSheet main window
  MainWindow.Create ();

  // Uninitalize COM
  if (sl)
    sl->Release ();
  CoUninitialize ();
}

int WINAPI
WinMain (HINSTANCE h,
	 HINSTANCE hPrevInstance, LPSTR command_line, int cmd_show)
{

  hinstance = h;

  // Make sure the C runtime functions use the same codepage as the GUI
  char locale[12];
  snprintf(locale, sizeof locale, ".%u", GetACP());
  setlocale(LC_ALL, locale);

  char **_argv;
  int argc;
  for (argc = 0, _argv = __argv; *_argv; _argv++)
    ++argc;
  _argv = __argv;

  try {
    bool help_option = false;
    bool invalid_option = false;
    char cwd[MAX_PATH];
    GetCurrentDirectory (MAX_PATH, cwd);
    local_dir = std::string (cwd);

    if (!GetOption::GetInstance ().Process (argc,_argv, NULL))
      help_option = invalid_option = true;
    else if (HelpOption)
      help_option = true;

    if (!((std::string) Arch).size ())
      {
#ifdef __x86_64__
	is_64bit = true;
#else
	is_64bit = false;
#endif
      }
    else if (((std::string) Arch).find ("64") != std::string::npos)
      is_64bit = true;
    else if (((std::string) Arch).find ("32") != std::string::npos
	     || ((std::string) Arch).find ("x86") != std::string::npos)
      is_64bit = false;
    else
      {
	char buff[80 + ((std::string) Arch).size ()];
	sprintf (buff, "Invalid option for --arch:  \"%s\"",
		 ((std::string) Arch).c_str ());
	fprintf (stderr, "*** %s\n", buff);
	exit (1);
      }

    if (GuiLangOption.isPresent())
      {
        // If option's value isn't numeric, perhaps we should try to interpret
        // it as a locale name?
        int rc = sscanf(((std::string)GuiLangOption).c_str(), "%hx", &langid);
        if (rc > 0)
          SetThreadUILanguage(langid);
      }

    unattended_mode = PackageManagerOption ? chooseronly
			: (UnattendedOption ? unattended : attended);

    bool output_only = help_option || VersionOption;

    SetupBaseName = SetupBaseNameOpt;
    SetupArch = is_64bit ? "x86_64" : "x86";
    SetupIniDir = SetupArch+"/";

    /* Initialize well known SIDs.  We need the admin SID to test if we're
       supposed to elevate. */
    nt_sec.initialiseWellKnownSIDs ();
    /* Check if we have to elevate. */
    bool elevate = !output_only && OSMajorVersion () >= 6
		   && !NoAdminOption && !nt_sec.isRunAsAdmin ();
    std::string elevate_extra_args;

    if (unattended_mode || output_only || !elevate)
      set_cout ();

    /* Start logging only if we don't elevate.  Same for setting default
       security settings. */
    LogSingleton::SetInstance (*LogFile::createLogFile ());
    const char *sep = isdirsep (local_dir[local_dir.size () - 1])
				? "" : "\\";
    /* Don't create log files for help or version output only. */
    if (!elevate && !output_only)
      {
	Logger ().setFile (LOG_BABBLE, local_dir + sep + "setup.log.full",
			   false);
	Logger ().setFile (0, local_dir + sep + "setup.log", true);
	Log (LOG_PLAIN) << "Starting cygwin install, version "
			<< setup_version << endLog;
      }

    if (help_option)
      {
	if (invalid_option)
	  Log (LOG_PLAIN) << "\nError during option processing.\n" << endLog;
        Log (LOG_PLAIN) << "Cygwin setup " << setup_version << endLog;
	Log (LOG_PLAIN) << "\nCommand Line Options:\n" << endLog;
	GetOption::GetInstance ().ParameterUsage (Log (LOG_PLAIN));
	Log (LOG_PLAIN) << endLog;
	Log (LOG_PLAIN) << "The default is to both download and install packages, unless either --download or --local-install is specified." << endLog;
	Logger ().exit (invalid_option ? 1 : 0, false);
	goto finish_up;
      }

    if (VersionOption)
      {
        Log (LOG_PLAIN) << "Cygwin setup " << setup_version << endLog;
        Logger ().exit (0, false);
        goto finish_up;
      }

    /* Check if Cygwin works on this Windows version */
    if (!UnsupportedOption && (OSMajorVersion () < 6))
      {
	mbox (NULL, IDS_UNSUPPORTED_WINDOWS_VERSION,
              MB_ICONEXCLAMATION | MB_OK);
	Logger ().exit (1, false);
      }

    /* Warn if Windows version is deprecated for Cygwin */
    if (!DeprecatedOption && !elevate)
      {
        if
#ifdef _X86_
          (TRUE)
#else
          (!is_64bit)
#endif
          mbox (NULL, IDS_DEPRECATED_WINDOWS_ARCH,
                MB_ICONEXCLAMATION | MB_OK | MB_DSA_CHECKBOX);
        else if ((OSMajorVersion () == 6) && (OSMinorVersion() < 1))
          mbox (NULL, IDS_DEPRECATED_WINDOWS_VERSION,
                MB_ICONEXCLAMATION | MB_OK | MB_DSA_CHECKBOX);
      }

    /* Set default DACL and Group. */
    nt_sec.setDefaultSecurity ((root_scope == IDC_ROOT_SYSTEM));

    /*
       If --symlink-type option isn't given, look for winsymlinks in CYGWIN
       env var for a default

       Since the current environment doesn't get passed to the process started
       with with ShellExecuteEx, we need to convert the env var into an option
       for that elevated instance.
    */
    if (!SymlinkTypeOption.isPresent()) {
      std::string cygwin;
      DWORD len = GetEnvironmentVariable ("CYGWIN", &cygwin[0], 0);
      cygwin.resize(len);
      GetEnvironmentVariable ("CYGWIN", &cygwin[0], len);

      if (cygwin.find("winsymlinks:native") != std::string::npos)
        {
          symlinkType = SymlinkTypeNative;
          elevate_extra_args.append("--symlink-type native");
        }
      else if (cygwin.find("winsymlinks:wsl") != std::string::npos)
        {
          symlinkType = SymlinkTypeWsl;
          elevate_extra_args.append("--symlink-type wsl");
        }
      else if (cygwin.find("winsymlinks:sys") != std::string::npos)
        {
          symlinkType = SymlinkTypeMagic;
          elevate_extra_args.append("--symlink-type sys");
        }
      else if (cygwin.find("winsymlinks:lnk") != std::string::npos)
        {
          // Ignore CYGWIN=winsymlinks:lnk, as '--symlink-type lnk' is not implemented
          // symlinkType = SymlinkTypeShortcut;
          // elevate_extra_args.append("--symlink-type lnk");
        }
      }
    else
      {
        symlinkType = (SymlinkTypeEnum)(int)SymlinkTypeOption;
      }

    if (symlinkType == SymlinkTypeWsl)
      {
        VersionInfo v = GetVer();
        if ((v.major() < 10) ||
            ((v.major() == 10) && (v.buildNumber() < 14393)))
          {
            fprintf (stderr, "*** --symlink-type wsl requires Windows 10 1607 or later\n");
            exit(1);
          }
      }
    else if (symlinkType == SymlinkTypeNative)
      {
        if (!(elevate || is_developer_mode() || nt_sec.hasSymlinkCreationRights()))
          {
            fprintf (stderr, "*** --symlink-type native requires SeCreateSymbolicLink privilege or 'Developer Mode'\n");
            exit(1);
          }
      }
    else if (symlinkType == SymlinkTypeShortcut)
      {
        fprintf (stderr, "*** --symlink-type lnk is not implemented\n");
        exit(1);
      }

    if (elevate)
      {
	char exe_path[MAX_PATH];
	if (!GetModuleFileName(NULL, exe_path, ARRAYSIZE(exe_path)))
	  goto finish_up;

	SHELLEXECUTEINFO sei = { sizeof(sei) };
	sei.lpVerb = "runas";
	sei.lpFile = exe_path;
	sei.nShow = SW_NORMAL;
	if (WaitOption)
	  sei.fMask |= SEE_MASK_NOCLOSEPROCESS;

	// Avoid another isRunAsAdmin check in the child.
	std::string command_line_cs (command_line);
	command_line_cs += " -";
	command_line_cs += NoAdminOption.shortOption();
	command_line_cs += " ";
	command_line_cs += elevate_extra_args;
	sei.lpParameters = command_line_cs.c_str ();

	if (ShellExecuteEx(&sei))
	  {
	    DWORD exitcode = 0;
	    /* Wait until child process is finished. */
	    if (WaitOption && sei.hProcess != NULL)
	      if (!WaitForSingleObject (sei.hProcess, INFINITE))
	        GetExitCodeProcess (sei.hProcess, &exitcode);
	    Logger ().setExitMsg (IDS_ELEVATED);
	    Logger ().exit (exitcode, false);
	  }
	Log (LOG_PLAIN) << "Starting elevated child process failed" << endLog;
	Logger ().exit (1, false);
      }
    else
      {
	UserSettings Settings;
        UserSettings::instance().load (local_dir);
	main_display ();
	Settings.save ();	// Clean exit.. save user options.
	if (rebootneeded)
	  Logger ().setExitMsg (IDS_REBOOT_REQUIRED);
	Logger ().exit (rebootneeded ? IDS_REBOOT_REQUIRED : 0);
      }
finish_up:
    ;
  }
  TOPLEVEL_CATCH(NULL, "main");

  // Never reached
  return 0;
}
