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

/* The purpose of this file is to download all the files we need to
   do the installation. */

#include "win32.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "resource.h"
#include "msg.h"
#include "ini.h"
#include "dialog.h"
#include "concat.h"
#include "geturl.h"
#include "state.h"
#include "mkdir.h"

#define pi (package[i].info[package[i].trust])

void
do_download (HINSTANCE h)
{
  int i;
  if (source == IDC_SOURCE_DOWNLOAD)
    next_dialog = 0;
  else
    next_dialog = IDD_S_INSTALL;

  for (i=0; i<npackages; i++)
    if (package[i].action != ACTION_SAME)
      {
	char *local = pi.install;

	struct stat s;
	if (stat (local, &s) >= 0)
	  if (s.st_size == pi.install_size)
	    continue;

	mkdir_p (0, local);

	if (get_url_to_file (concat (MIRROR_SITE, "/", pi.install, 0),
			     concat (local, ".tmp", 0),
			     pi.install_size))
	  {
	    package[i].action = ACTION_ERROR;
	    continue;
	  }
	else
	  {
	    stat (concat (local, ".tmp", 0), &s);
	    if (s.st_size == pi.install_size)
	      {
		rename (concat (local, ".tmp", 0), local);
	      }
	    else
	      {
		note (IDS_DOWNLOAD_SHORT, local, s.st_size, pi.install_size);
		package[i].action = ACTION_ERROR;
	      }
	  }
      }

  dismiss_url_status_dialog ();
}
