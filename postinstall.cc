/*
 * Copyright (c) 2000, 2001 Red Hat, Inc.
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

/* The purpose of this file is to run all the post-install scripts
   in their various forms. */

#include "dialog.h"
#include "find.h"
#include "mount.h"
#include "script.h"
#include "state.h"
#include "FindVisitor.h"
#include "package_db.h"
#include "package_meta.h"
#include "resource.h"
#include "threebar.h"
#include "Exception.h"
#include "postinstallresults.h"

#include <algorithm>
#include <sstream>

using namespace std;

extern ThreeBarProgressPage Progress;
extern PostInstallResultsPage PostInstallResults;

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------

class RunFindVisitor : public FindVisitor
{
public:
  RunFindVisitor (vector<Script> *scripts, const std::string& stratum = "")
    : _scripts(scripts),
      stratum(stratum)
  {}
  virtual void visitFile(const std::string& basePath,
                         const WIN32_FIND_DATA *theFile)
    {
      std::string fn = std::string("/etc/postinstall/") + theFile->cFileName;
      Script script(fn);
      if (script.not_p(stratum))
	  _scripts->push_back(Script (fn));
    }
  virtual ~ RunFindVisitor () {}
protected:
  RunFindVisitor (RunFindVisitor const &);
  RunFindVisitor & operator= (RunFindVisitor const &);
private:
  vector<Script> *_scripts;
  const std::string stratum;
};

class PerpetualFindVisitor : public FindVisitor
{
public:
  PerpetualFindVisitor (vector<Script> *scripts, const string& stratum)
    : _scripts(scripts),
      stratum(stratum)
  {}
  virtual void visitFile(const std::string& basePath,
                         const WIN32_FIND_DATA *theFile)
    {
      std::string fn = std::string("/etc/postinstall/") + theFile->cFileName;
      Script script(fn);
      if (script.is_p(stratum))
	  _scripts->push_back(Script (fn));
    }
  virtual ~ PerpetualFindVisitor () {}
protected:
  PerpetualFindVisitor (PerpetualFindVisitor const &);
  PerpetualFindVisitor & operator= (PerpetualFindVisitor const &);
private:
  vector<Script> *_scripts;
  const std::string stratum;
};

// ---------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------

class RunScript
{
public:
  RunScript(const std::string& name, const vector<Script> &scripts) : _name(name), _scripts(scripts), _cnt(0)
    {
      Progress.SetText2 (name.c_str());
      Progress.SetBar1 (0, _scripts.size());
    }
  virtual ~RunScript()
    {
      Progress.SetText3 ("");
    }
  int run_one(Script const &aScript)
    {
      int retval;
      Progress.SetText3 (aScript.fullName().c_str());
      retval = aScript.run();
      ++_cnt;
      Progress.SetBar1 (_cnt, _scripts.size());
      return retval;
    }
  void run_all(std::string &s)
  {
    bool package_name_recorded = FALSE;

    for (std::vector <Script>::const_iterator j = _scripts.begin();
         j != _scripts.end();
         j++)
      {
        int retval = run_one(*j);

        if ((retval != 0) && (retval != -ERROR_INVALID_DATA))
          {
            if (!package_name_recorded)
              {
                s = s + "Package: " + _name + "\r\n";
                package_name_recorded = TRUE;
              }

            std::ostringstream fs;
            fs << "\t" <<  j->baseName() << " exit code " << retval << "\r\n";
            s = s + fs.str();
          }
      }
  }
private:
  std::string _name;
  const vector<Script> &_scripts;
  int _cnt;
};

static std::string
do_postinstall_thread (HINSTANCE h, HWND owner)
{
  Progress.SetText1 ("Running...");
  Progress.SetText2 ("");
  Progress.SetText3 ("");
  Progress.SetBar1 (0, 1);
  Progress.SetBar2 (0, 1);

  packagedb db;
  vector<packagemeta*> packages;
  PackageDBConnectedIterator i = db.connectedBegin ();
  while (i != db.connectedEnd ())
    {
      packagemeta & pkg = **i;
      if (pkg.installed)
	packages.push_back(&pkg);
      ++i;
    }

  const std::string postinst = cygpath ("/etc/postinstall");
  const std::string strata("0_z");
  std::string s = "";
  // iterate over all strata
  for (std::string::const_iterator it = strata.begin(); it != strata.end(); ++it)
    {
      const std::string sit(1, *it);
  // Look for any scripts in /etc/postinstall which should always be run
  vector<Script> perpetual;
  PerpetualFindVisitor myPerpetualVisitor (&perpetual, sit);
  Find (postinst).accept (myPerpetualVisitor);
  // sort the list alphabetically, assumes ASCII names only
  sort (perpetual.begin(), perpetual.end());
  // and try to run what we've found
  {
    RunScript scriptRunner(sit + "/Perpetual", perpetual);
    scriptRunner.run_all(s);
  }
  // For each package we installed, we noted anything installed into /etc/postinstall.
  // run those scripts now
  int numpkg = packages.size() + 1;
  int k = 0;
  for (vector <packagemeta *>::iterator  i = packages.begin (); i != packages.end (); ++i)
    {
      packagemeta & pkg = **i;

      vector<Script> installed = pkg.scripts();
      vector<Script> run;
      // extract non-perpetual scripts for the current stratum
      for (vector <Script>::iterator  j = installed.begin(); j != installed.end(); j++)
	{
	  if ((*j).not_p(sit))
	    run.push_back(*j);
	}

      RunScript scriptRunner(sit + "/" + pkg.name, run);
      scriptRunner.run_all(s);

      Progress.SetBar2 (++k, numpkg);
    }
  // Look for runnable non-perpetual scripts in /etc/postinstall.
  // This happens when a script from a previous install failed to run.
  vector<Script> scripts;
  RunFindVisitor myVisitor (&scripts, sit);
  Find (postinst).accept (myVisitor);
  // Remove anything which we just tried to run (so we don't try twice)
  for (vector <packagemeta *>::iterator i = packages.begin (); i != packages.end (); ++i)
    {
       packagemeta & pkg = **i;
       for (std::vector<Script>::const_iterator j = pkg.scripts().begin();
            j != pkg.scripts().end();
            j++)
         {
           std::vector<Script>::iterator p = find(scripts.begin(), scripts.end(), *j);
           if (p != scripts.end())
             {
               scripts.erase(p);
             }
         }
    }
  // and try to run what's left...
  {
    RunScript scriptRunner(sit + "/Unknown package", scripts);
    scriptRunner.run_all(s);
  }

  Progress.SetBar2 (numpkg, numpkg);

    }
  return s;
}

static DWORD WINAPI
do_postinstall_reflector (void *p)
{
  HANDLE *context;
  context = (HANDLE *) p;

  try
  {
    std::string s = do_postinstall_thread ((HINSTANCE) context[0], (HWND) context[1]);

    // Tell the postinstall results page the results string
    PostInstallResults.SetResultsString(s);

    // Tell the progress page that we're done running scripts
    Progress.PostMessageNow (WM_APP_POSTINSTALL_THREAD_COMPLETE, 0,
                          s.empty() ? IDD_DESKTOP : IDD_POSTINSTALL);
  }
  TOPLEVEL_CATCH((HWND) context[1], "postinstall");

  /* Revert primary group to admins group.  This allows to create all the
     state files written by setup as admin group owned. */
  if (root_scope == IDC_ROOT_SYSTEM)
    nt_sec.setAdminGroup ();

  ExitThread(0);
}

static HANDLE context[2];

void
do_postinstall (HINSTANCE h, HWND owner)
{
  /* Switch back to original primary group.  Otherwise we end up with a
     broken passwd entry for the current user.
     FIXME: Unfortunately this has the unfortunate side-effect that *all*
     files created via postinstall are group owned by the original primary
     group of the user.  Find a way to avoid this at one point. */
  if (root_scope == IDC_ROOT_SYSTEM)
    nt_sec.resetPrimaryGroup ();

  context[0] = h;
  context[1] = owner;

  DWORD threadID;
  CreateThread (NULL, 0, do_postinstall_reflector, context, 0, &threadID);
}

