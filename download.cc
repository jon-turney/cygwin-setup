/*
 * Copyright (c) 2000, 2001, Red Hat, Inc.
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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"

#include <stdio.h>
#include <unistd.h>

#include "resource.h"
#include "msg.h"
#include "ini.h"
#include "dialog.h"
#include "concat.h"
#include "geturl.h"
#include "state.h"
#include "mkdir.h"
#include "log.h"
#include "port.h"

DWORD get_file_size (const char *name)
{
  HANDLE h;
  WIN32_FIND_DATA buf;
  DWORD ret = 0;

  h = FindFirstFileA (name, &buf);
  if (h != INVALID_HANDLE_VALUE)
    {
      if (buf.nFileSizeHigh == 0)
	ret = buf.nFileSizeLow;
      FindClose (h);
    }
  return ret;
}

static int
download_one (char *name, unsigned int expected_size, int action)
{
  char *local = name;

  DWORD size;
  if ((size = get_file_size (local)) > 0)
    if (size == expected_size && action != ACTION_SRC_ONLY
	&& action != ACTION_REDO)
      return 0;

  mkdir_p (0, local);

  if (get_url_to_file (concat (MIRROR_SITE, "/", name, 0),
		       concat (local, ".tmp", 0), expected_size))
    {
      note (IDS_DOWNLOAD_FAILED, name);
      return 1;
    }
  else
    {
      size = get_file_size (concat (local, ".tmp", 0));
      if (size == expected_size)
	{
	  log (0, "Downloaded %s", local);
	  if (_access (local, 0) == 0)
	    remove (local);
	  rename (concat (local, ".tmp", 0), local);
	}
      else
	{
	  log (0, "Download %s wrong size (%ld actual vs %d expected)",
	       local, size, expected_size);
	  note (IDS_DOWNLOAD_SHORT, local, size, expected_size);
	  return 1;
	}
    }

  return 0;
}

void
do_download (HINSTANCE h)
{
  int errors = 0;
  total_download_bytes = 0;
  total_download_bytes_sofar = 0;

  for (Package * pkg = package; pkg->name; pkg++)
    if (is_download_action (pkg))
      {
	Info *pi = pkg->info + pkg->trust;
	DWORD size = get_file_size (pi->install);
	char *local = pi->install;
	if (pkg->action != ACTION_SRC_ONLY &&
	    (pkg->action == ACTION_REDO || size != pi->install_size))
	  total_download_bytes += pi->install_size;
	local = pi->source;
	size = get_file_size (pi->source);
	if (pkg->srcpicked &&
	    (pkg->action == ACTION_SRC_ONLY || size != pi->source_size))
	  total_download_bytes += pi->source_size;
      }

  for (Package * pkg = package; pkg->name; pkg++)
    if (is_download_action (pkg))
      {
	int e = 0;
	Info *pi = pkg->info + pkg->trust;
	if (pkg->action != ACTION_SRC_ONLY)
	  e += download_one (pi->install, pi->install_size, pkg->action);
	if (pkg->srcpicked && pi->source)
	  e += download_one (pi->source, pi->source_size, pkg->action);
	errors += e;
	if (e)
	  pkg->action = ACTION_ERROR;
      }

  dismiss_url_status_dialog ();

  if (errors)
    {
      if (yesno (IDS_DOWNLOAD_INCOMPLETE) == IDYES)
	{
	  next_dialog = IDD_SITE;
	  return;
	}
    }

  if (source == IDC_SOURCE_DOWNLOAD)
    {
      if (errors)
	exit_msg = IDS_DOWNLOAD_INCOMPLETE;
      else
	exit_msg = IDS_DOWNLOAD_COMPLETE;
      next_dialog = 0;
    }
  else
    next_dialog = IDD_S_INSTALL;
}
