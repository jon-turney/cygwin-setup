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

class RunFindVisitor : public FindVisitor
{
public:
  RunFindVisitor (){}
  virtual void visitFile(String const &basePath, const WIN32_FIND_DATA *theFile)
    {
      run_script ("/etc/postinstall/", theFile->cFileName);
    }
  virtual ~ RunFindVisitor () {}
protected:
  RunFindVisitor (RunFindVisitor const &);
  RunFindVisitor & operator= (RunFindVisitor const &);
};
  
void
do_postinstall (HINSTANCE h, HWND owner)
{
  next_dialog = 0;
  init_run_script ();
  SetCurrentDirectory (get_root_dir ().cstr_oneuse());
  packagedb db;
  PackageDBConnectedIterator i = db.connectedBegin ();
  while (i != db.connectedEnd ())
    {
      packagemeta & pkg = **i;
      if (pkg.installed)
	for (std::vector<Script>::iterator script=pkg.installed.scripts().begin(); script != pkg.installed.scripts().end(); ++script) 
	  run_script ("/etc/postinstall/", script->baseName());
      ++i;
    }
  RunFindVisitor myVisitor;
  ExcludeNameFilter notDone("*.done");
  FilterVisitor excludeDoneVisitor(&myVisitor, &notDone);
  String postinst = cygpath ("/etc/postinstall");
  Find (postinst).accept (excludeDoneVisitor);
}
