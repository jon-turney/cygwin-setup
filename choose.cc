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

/* The purpose of this file is to let the user choose which packages
   to install, and which versions of the package when more than one
   version is provided.  The "trust" level serves as an indication as
   to which version should be the default choice.  At the moment, all
   we do is compare with previously installed packages to skip any
   that are already installed (by setting the action to ACTION_SAME).
   While the "trust" stuff is supported, it's not really implemented
   yet.  We always prefer the "current" option.  In the future, this
   file might have a user dialog added to let the user choose to not
   install packages, or to install packages that aren't installed by
   default. */

#include "win32.h"
#include <stdio.h>
#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "ini.h"
#include "concat.h"
#include "msg.h"

void
do_choose (HINSTANCE h)
{
  int trust_prefs[NTRUST];
  int i, t;

  /* support this later */
  trust_level = TRUST_CURR;

  t = 0;
  for (i=trust_level; i>=0; i--)
    trust_prefs[t++] = i;
  for (i=trust_level+1; i<NTRUST; i++)
    trust_prefs[t++] = i;

  for (i=0; i<npackages; i++)
    {
      for (t=0; t<NTRUST; t++)
	if (package[i].info[t].install)
	  {
	    package[i].trust = t;
	    break;
	  }
    }

  if (root_dir)
    {
      char line[1000], pkg[1000], inst[1000], src[1000];
      int instsz, srcsz;
      FILE *db = fopen (concat (root_dir, "/etc/setup/installed.db", 0), "rt");
      if (db)
	{
	  while (fgets (line, 1000, db))
	    {
	      src[0] = 0;
	      srcsz = 0;
	      sscanf (line, "%s %s %d %s %d", pkg, inst, &instsz, src, &srcsz);

	      for (i=0; i<npackages; i++)
		{
		  if (strcmp (package[i].name, pkg) == 0)
		    {
		      if (strcmp (package[i].info[package[i].trust].install,
				  inst) == 0)
			package[i].action = ACTION_SAME;
		      else
			package[i].action = ACTION_UPGRADE;
		    }
		}
	    }
	  fclose (db);
	}
    }

  for (i=0; i<npackages; i++)
    if (package[i].action == ACTION_UNKNOWN)
      package[i].action = ACTION_NEW;

  if (source == IDC_SOURCE_CWD)
    next_dialog = IDD_S_INSTALL;
  else
    next_dialog = IDD_S_DOWNLOAD;
}

