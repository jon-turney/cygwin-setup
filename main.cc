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
#include "LogFile.h"
#include "version.h"

#include "port.h"
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
#include "threebar.h"
#include "desktop.h"

#include "getopt++/GetOption.h"
#include "getopt++/BoolOption.h"

int next_dialog;

HINSTANCE hinstance;

static BoolOption UnattendedOption (false, 'q', "quiet-mode", "Unattended setup mode");

/* Maximum size of a SID on NT/W2K. */
#define MAX_SID_LEN	40

/* Computes the size of an ACL in relation to the number of ACEs it
   should contain. */
#define TOKEN_ACL_SIZE(cnt) (sizeof(ACL) + \
			     (cnt) * (sizeof(ACCESS_ALLOWED_ACE) + MAX_SID_LEN))

#define iswinnt		(GetVersion() < 0x80000000)

namespace Setup {
  class SIDWrapper {
    public:
      SIDWrapper();
      /* Prevent synthetics. If assignment is needed, this should be refcounting  */
      SIDWrapper(SIDWrapper const &);
      SIDWrapper&operator=(SIDWrapper const&);
      ~SIDWrapper();
      /* We could look at doing weird typcast overloads here,
      but manual access is easier for now
      */
      PSID &theSID();
      PSID const &theSID() const;
    private:
      PSID value;
  };

SIDWrapper::SIDWrapper() : value(NULL) {}
SIDWrapper::~SIDWrapper()
{
  if (value)
    FreeSid (value);
}

PSID &
SIDWrapper::theSID()
{
  return value;
}

PSID const &
SIDWrapper::theSID() const
{
  return value;
}

  class HANDLEWrapper {
    public:
      HANDLEWrapper();
      /* Prevent synthetics. If assignment is needed, we should duphandles, or refcount */
      HANDLEWrapper(HANDLEWrapper const &);
      HANDLEWrapper&operator=(HANDLEWrapper const &);
      ~HANDLEWrapper();
      HANDLE &theHANDLE();
      HANDLE const &theHANDLE() const;
    private:
      HANDLE value;
  };

HANDLEWrapper::HANDLEWrapper() : value (NULL) {}
HANDLEWrapper::~HANDLEWrapper()
{
  if (value)
    CloseHandle(value);
}

HANDLE &
HANDLEWrapper::theHANDLE() 
{
  return value;
}

HANDLE const &
HANDLEWrapper::theHANDLE() const 
{
  return value;
}

};

class NTSecurity
{
public:
  NTSecurity();
  ~NTSecurity();
  /* prevent synthetics */
  NTSecurity &operator=(NTSecurity const&);
  NTSecurity(NTSecurity const &);

  void setDefaultSecurity();
private:
  void failed(bool const &);
  bool const &failed() const;
  void initialiseEveryOneSID();
  void setDefaultDACL ();
  Setup::SIDWrapper everyOneSID, asid, usid;
  Setup::HANDLEWrapper token;
  bool failed_;
};

void
set_default_sec()
{
  NTSecurity worker;
  worker.setDefaultSecurity();
}

NTSecurity::NTSecurity() : everyOneSID (), asid(), usid(), token(), failed_(false)
{}

NTSecurity::~NTSecurity()
{
}

void
NTSecurity::failed(bool const &aBool) 
{
  failed_ = aBool;
}

bool const &
NTSecurity::failed() const
{
  return failed_;
}

void
NTSecurity::initialiseEveryOneSID()
{
  SID_IDENTIFIER_AUTHORITY sid_auth = { SECURITY_WORLD_SID_AUTHORITY };
  if (!AllocateAndInitializeSid (&sid_auth, 1, 0, 0, 0, 0, 0, 0, 0, 0, &everyOneSID.theSID()))
    {
      log (LOG_TIMESTAMP) << "AllocateAndInitializeSid() failed: " <<
	   GetLastError () << endLog;
      failed(true);
    }
}

void
NTSecurity::setDefaultDACL ()
{
  /* To assure that the created files have a useful ACL, the 
     default DACL in the process token is set to full access to
     everyone. This applies to files and subdirectories created
     in directories which don't propagate permissions to child
     objects. 
     To assure that the files group is meaningful, a token primary
     group of None is changed to Users or Administrators. */

  /* Create a buffer which has enough room to contain the TOKEN_DEFAULT_DACL
     structure plus an ACL with one ACE. */
  char buf[sizeof (TOKEN_DEFAULT_DACL) + TOKEN_ACL_SIZE (1)];

  /* First initialize the TOKEN_DEFAULT_DACL structure. */
  PTOKEN_DEFAULT_DACL dacl = (PTOKEN_DEFAULT_DACL) buf;
  dacl->DefaultDacl = (PACL) (buf + sizeof *dacl);

  /* Initialize the ACL for containing one ACE. */
  if (!InitializeAcl (dacl->DefaultDacl, TOKEN_ACL_SIZE (1), ACL_REVISION))
    {
      log (LOG_TIMESTAMP) << "InitializeAcl() failed: " << GetLastError ()
	<< endLog;
      failed(true);
      return;
    }

  initialiseEveryOneSID();
  if (failed())
    return;

  /* Create the ACE which grants full access to "Everyone" and store it
     in dacl->DefaultDacl. */
  if (!AddAccessAllowedAce
      (dacl->DefaultDacl, ACL_REVISION, GENERIC_ALL, everyOneSID.theSID()))
    {
      log (LOG_TIMESTAMP) << "AddAccessAllowedAce() failed: %lu" << 
	   GetLastError () << endLog;
      failed(true);
      return;
    }

  /* Get the processes access token. */
  if (!OpenProcessToken (GetCurrentProcess (),
			 TOKEN_READ | TOKEN_ADJUST_DEFAULT, &token.theHANDLE()))
    {
      log (LOG_TIMESTAMP) << "OpenProcessToken() failed: " << 
	GetLastError () << endLog;
      failed(true);
      return;
    }

  /* Set the default DACL to the above computed ACL. */
  if (!SetTokenInformation (token.theHANDLE(), TokenDefaultDacl, dacl, sizeof buf))
    {
      log (LOG_TIMESTAMP) << "SetTokenInformation() failed: " << 
        GetLastError () << endLog;
      failed(true);
    }
}

void
NTSecurity::setDefaultSecurity ()
{

  setDefaultDACL();
  if (failed())
    return;

  /* Get the default group */
  struct {
    PSID psid;
    char buf[MAX_SID_LEN];
  } gsid;
  DWORD size;
  if (!GetTokenInformation (token.theHANDLE(), TokenPrimaryGroup, &gsid, sizeof gsid, &size))
    {
      log (LOG_TIMESTAMP) << "GetTokenInformation() failed: " << 
	GetLastError () << endLog;
      return;
    }

  /* Get the computer name */
  char compname[MAX_COMPUTERNAME_LENGTH + 1];
  size = sizeof (compname);
  if (!GetComputerName (compname, &size))
    {
      log (LOG_TIMESTAMP) << "GetComputerName() failed: " << 
	GetLastError () << endLog;
      return;
    }

  /* Get the local domain SID */
  SID_NAME_USE use;
  DWORD sz;
  char domain[MAX_COMPUTERNAME_LENGTH + 1];
  char lsid[MAX_SID_LEN];
  size = sizeof (lsid);
  sz = sizeof (domain);
  if (!LookupAccountName (NULL, compname, lsid, &size, 
			  domain, &sz, &use)) 
    {
      log (LOG_TIMESTAMP) << "LookupAccountName() failed: " << 
	GetLastError () << endLog;
      return;
    }

  /* Create the None SID from the domain SID.
     On NT the last subauthority of a domain is -1 and it is replaced by the RID.
     On other systems the RID is appended. */
  sz = *GetSidSubAuthorityCount (lsid);
  if (*GetSidSubAuthority (lsid, sz -1) != (DWORD) -1) 
    *GetSidSubAuthorityCount (lsid) = ++sz;
  *GetSidSubAuthority (lsid, sz -1) = DOMAIN_GROUP_RID_USERS;
  
  /* See if the group is None */
  if (!EqualSid (gsid.psid, lsid))
    return;

  bool isadmins = false, isusers = false;
  SID_IDENTIFIER_AUTHORITY sid_auth;
  sid_auth = (SID_IDENTIFIER_AUTHORITY) { SECURITY_NT_AUTHORITY };
  /* Get the SID for "Administrators" S-1-5-32-544 */
  if (!AllocateAndInitializeSid (&sid_auth, 2, SECURITY_BUILTIN_DOMAIN_RID, 
				 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &asid.theSID()))
    {
	log (LOG_TIMESTAMP) << "AllocateAndInitializeSid() failed: " <<
	GetLastError () << endLog;
	return;
    }
  /* Get the SID for "Users" S-1-5-32-545 */
  if (!AllocateAndInitializeSid (&sid_auth, 2, SECURITY_BUILTIN_DOMAIN_RID, 
			DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &usid.theSID()))
    {
      log (LOG_TIMESTAMP) << "AllocateAndInitializeSid() failed: " <<
			GetLastError () << endLog;
      return;
    }
  /* Get the token groups */
  if (!GetTokenInformation (token.theHANDLE(), TokenGroups, NULL, 0, &size)
	  && GetLastError () != ERROR_INSUFFICIENT_BUFFER)
    {
      log (LOG_TIMESTAMP) << "GetTokenInformation() failed: " <<
	    GetLastError () << endLog;
      return;
    }
  else 
    {
      char buf[size];
      TOKEN_GROUPS *groups = (TOKEN_GROUPS *) buf;

      if (!GetTokenInformation (token.theHANDLE(), TokenGroups, buf, size, &size))
        {
          log (LOG_TIMESTAMP) << "GetTokenInformation() failed: " <<
	  	GetLastError () << endLog;
	  return;
	}
      else
        /* See if admins or users is present */
        for (DWORD pg = 0; pg < groups->GroupCount; ++pg)
          {
	    isadmins = isadmins || EqualSid(groups->Groups[pg].Sid, asid.theSID());
	    isusers = isusers || EqualSid(groups->Groups[pg].Sid, usid.theSID());
	  }
    }
  /* Set the default group to one of the above computed SID. */
  PSID nsid = NULL;
  if (isusers)
    {
      nsid = usid.theSID();
      log(LOG_TIMESTAMP) << "Changing gid to Users" << endLog;
    }
  else if (isadmins)
    {
      nsid = asid.theSID();
      log(LOG_TIMESTAMP) << "Changing gid to Administrators" << endLog;
    }
  if (nsid && !SetTokenInformation (token.theHANDLE(), TokenPrimaryGroup, &nsid, sizeof nsid))
    log (LOG_TIMESTAMP) << "SetTokenInformation() failed: " << 
	  GetLastError () << endLog;
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

  char *cwd=new char[_MAX_PATH];
  GetCurrentDirectory (_MAX_PATH, cwd);
  local_dir = String (cwd);
  delete cwd;

  LogSingleton::SetInstance (*(theLog = LogFile::createLogFile()));
  theLog->setFile (LOG_BABBLE, local_dir + "/setup.log.full", false);
  theLog->setFile (0, local_dir + "/setup.log", true);

  next_dialog = IDD_SPLASH;

  log (LOG_PLAIN) << "Starting cygwin install, version " << version << endLog;

  SplashPage Splash;
  AntiVirusPage AntiVirus;
  SourcePage Source;
  RootPage Root;
  LocalDirPage LocalDir;
  NetPage Net;
  SitePage Site;
  ChooserPage Chooser;
  DesktopSetupPage Desktop;
  PropSheet MainWindow;

  log (LOG_TIMESTAMP) << "Current Directory: " << local_dir << endLog;

  // TODO: make an equivalent for __argv under cygwin.
  char **_argv;
#ifndef __CYGWIN__
  int argc;
//  char **_argv;
#ifndef __CYGWIN__
  for (argc = 0, _argv = __argv; *_argv; _argv++)++argc;
  _argv = __argv;
#else
//  for (argc = 0, _argv = argv; *_argv; _argv++)++argc;
  _argv = argv;
#endif
#else
  _argv = argv;
#endif

  if (!GetOption::GetInstance().Process (argc,_argv))
    theLog->exit(1);
// #endif

  unattended_mode = UnattendedOption;

  /* Set the default DACL and Group only on NT/W2K. 9x/ME has 
     no idea of access control lists and security at all. */
  if (iswinnt)
    set_default_sec ();

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
  MainWindow.AddPage (&Progress);
  MainWindow.AddPage (&Desktop);

  // Create the PropSheet main window
  MainWindow.Create ();

  theLog->exit (0);
  /* Keep gcc happy :} */
  return 0;
}
