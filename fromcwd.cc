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

/* The purpose of this file is to handle the case where we're
   installing from files that already exist in the current directory.
   If a setup.ini file is present, we set the mirror site to "." and
   pretend we're installing from the `internet' ;-) else we have to
   find all the .tar.gz files, deduce their versions, and try to
   compare versions in the case where the current directory contains
   multiple versions of any given package.  We do *not* try to compare
   versions with already installed packages; we always choose a
   package in the current directory over one that's already installed
   (otherwise, why would you have asked to install it?).  Note
   that we search recursively. */


#include "String++.h"
#include "find.h"
#include "ini.h"

#include "FindVisitor.h"
#include "IniDBBuilderPackage.h"
#include "IniParseFeedback.h"

class SetupFindVisitor : public FindVisitor
{
public:
  SetupFindVisitor () : inidir (false)
  {
    found_ini.resize (setup_ext_list.size ());
    found_ini.assign (setup_ext_list.size (), false);
  }
  virtual void visitFile (const std::string& basePath,
			  const WIN32_FIND_DATA *theFile)
  {
    if (inidir &&
	(theFile->nFileSizeLow || theFile->nFileSizeHigh))
      {
	std::vector<bool>::iterator fi = found_ini.begin ();
	for (std::vector<std::string>::const_iterator ext = setup_ext_list.begin ();
	     ext != setup_ext_list.end ();
	     ext++, fi++)
	  {
	    if (!casecompare (SetupBaseName + "." + *ext,  theFile->cFileName))
	      *fi = true;
	  }
      }
  }
  virtual void visitDirectory (const std::string& basePath,
			       WIN32_FIND_DATA const *aDir, int level)
  {
    if (level <= 0)
      return;
    inidir = !casecompare (SetupArch, aDir->cFileName);
    if (level == 1 && !inidir)
      return;
    Find aFinder (basePath + aDir->cFileName);
    aFinder.accept (*this, inidir ? 0 : --level);
	std::vector<bool>::const_iterator fi = found_ini.begin ();
	for (std::vector<std::string>::const_iterator ext = setup_ext_list.begin ();
	     ext != setup_ext_list.end ();
	     ext++, fi++)
	  {
	    if (*fi)
	      {
		found_ini_list.push_back (basePath + SetupArch + "/"
					  + SetupBaseName + "." + *ext);
		/* 
		 * Terminate the search after the first setup file
		 * found, which shadows any setup files with
		 * extensions later in the preference order in the
		 * same directory.
		 *
		 * FIXME: It would probably be more sensible to return
		 * all matches (perhaps one list per directory) and
		 * let do_local_ini pick the first one that parses
		 * correctly, just like do_remote_ini does.
		 */
		break;
	      }
	  }
	found_ini.assign (setup_ext_list.size (), false);
  }
  virtual ~ SetupFindVisitor (){}
  operator bool () const
  {
    return !found_ini_list.empty ();
  }
protected:
  SetupFindVisitor (SetupFindVisitor const &);
  SetupFindVisitor & operator= (SetupFindVisitor const &);
private:
  bool inidir;
  std::vector<bool> found_ini;
};

IniList found_ini_list;

bool
do_from_local_dir (HINSTANCE h, HWND owner, std::string &local_dir)
{
  SetupFindVisitor found;
  // single mirror?
  Find (local_dir.c_str ()).accept (found, 1);
  if (found)
      return true;
  // multi-mirror?
  Find (local_dir.c_str ()).accept (found, 2);
  if (found)
      return true;
  return false;
}
