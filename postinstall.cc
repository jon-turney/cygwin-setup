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

#include "win32.h"

#include <stdlib.h>

#include "state.h"
#include "dialog.h"
#include "find.h"
#include "mount.h"
#include "script.h"

static void
run_script_in_etc_postinstall (char *fname, unsigned int size)
{
  run_script ("/etc/postinstall/", fname);
}

void
do_postinstall (HINSTANCE h, HWND owner)
{
  next_dialog = 0;
  init_run_script ();
  SetCurrentDirectory (get_root_dir ());
  find (cygpath ("/etc/postinstall", 0), run_script_in_etc_postinstall);
}
