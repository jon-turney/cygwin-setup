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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "dialog.h"
#include "find.h"
#include "mount.h"
#include "script.h"
#include "FindVisitor.h"
#include "FilterVisitor.h"
#include "package_db.h"
#include "package_meta.h"
#include "resource.h"
#include "threebar.h"

#include <algorithm>

using namespace std;

extern ThreeBarProgressPage Progress;

class RunFindVisitor : public FindVisitor
{
public:
  RunFindVisitor (vector<Script> *scripts) : _scripts(scripts) {}
  virtual void visitFile(String const &basePath, const WIN32_FIND_DATA *theFile)
    {
      String fn = String("/etc/postinstall/")+theFile->cFileName;
      _scripts->push_back(Script (fn));
    }
  virtual ~ RunFindVisitor () {}
protected:
  RunFindVisitor (RunFindVisitor const &);
  RunFindVisitor & operator= (RunFindVisitor const &);
private:
  vector<Script> *_scripts;
};

class RunScript : public unary_function<Script const &, void>
{
public:
  RunScript(String const &name, int num) : _num(num), _cnt(0)
    {
      Progress.SetText2 (name.cstr_oneuse());
      Progress.SetBar1 (_cnt, _num);
    }
  virtual ~RunScript()
    {
      Progress.SetText3 ("");
    }
  void operator() (Script const &aScript) 
    {
      Progress.SetText3 (aScript.fullName().cstr_oneuse());
      aScript.run();
      ++_cnt;
      Progress.SetBar1 (_cnt, _num);
    }
private:
  int _num;
  int _cnt;
};

static void
do_postinstall_thread (HINSTANCE h, HWND owner)
{
  Progress.SetText1 ("Running...");
  Progress.SetText2 ("");
  Progress.SetText3 ("");
  Progress.SetBar1 (0, 1);
  Progress.SetBar2 (0, 1);

  init_run_script ();
  SetCurrentDirectory (get_root_dir ().cstr_oneuse());
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
  int numpkg = packages.size() + 1;
  int k = 0;
  for (i = packages.begin (); i != packages.end (); ++i)
    {
      packagemeta & pkg = **i;
      for_each (pkg.installed.scripts().begin(), pkg.installed.scripts().end(),
		RunScript(pkg.name, pkg.installed.scripts().size()));
      ++k;
      Progress.SetBar2 (k, numpkg);
    }
  ExcludeNameFilter notDone("*.done");
  String postinst = cygpath ("/etc/postinstall");
  vector<Script> scripts;
  RunFindVisitor myVisitor (&scripts);
  FilterVisitor excludeDoneVisitor(&myVisitor, &notDone);
  Progress.SetBar1 (0, 1);
  Find (postinst).accept (excludeDoneVisitor);
  for_each (scripts.begin(), scripts.end(),
	    RunScript("No package", scripts.size()));
  Progress.SetBar2 (numpkg, numpkg);
}

static DWORD WINAPI
do_postinstall_reflector (void *p)
{
  HANDLE *context;
  context = (HANDLE *) p;

  do_postinstall_thread ((HINSTANCE) context[0], (HWND) context[1]);

  // Tell the progress page that we're done running scripts
  Progress.PostMessage (WM_APP_POSTINSTALL_THREAD_COMPLETE);

  ExitThread(0);
}

static HANDLE context[2];

void
do_postinstall (HINSTANCE h, HWND owner)
{
  context[0] = h;
  context[1] = owner;

  DWORD threadID;
  CreateThread (NULL, 0, do_postinstall_reflector, context, 0, &threadID);
}

