/*
 * Copyright (c) 2001, Jan Nieuwenhuizen.
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
 *            Jan Nieuwenhuizen <janneke@gnu.org>
 *
 */

/* The purpose of this file is to provide functions for the invocation
   of install scripts. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "log.h"
#include "filemanip.h"
#include "mount.h"
#include "io_stream.h"

static String sh = String();
static const char *cmd = 0;
static OSVERSIONINFO verinfo;

static const char *shells[] = {
  "/bin/sh.exe",
  "/usr/bin/sh.exe",
  "/bin/bash.exe",
  "/usr/bin/bash.exe",
  0
};

void
init_run_script ()
{
  for (int i = 0; shells[i]; i++)
    {
      sh = backslash (cygpath (shells[i]));
      if (_access (sh.cstr_oneuse(), 0) == 0)
	break;
      sh = String();
    }
  
  char old_path[_MAX_PATH];
  GetEnvironmentVariable ("PATH", old_path, sizeof (old_path));
  SetEnvironmentVariable ("PATH", backslash (cygpath ("/bin") + ";" +
					     cygpath ("/usr/bin") + ";" +
					     old_path).cstr_oneuse());
  SetEnvironmentVariable ("CYGWINROOT", get_root_dir ().cstr_oneuse());

  verinfo.dwOSVersionInfoSize = sizeof (verinfo);
  GetVersionEx (&verinfo);

  switch (verinfo.dwPlatformId)
    {
    case VER_PLATFORM_WIN32_NT:
      cmd = "cmd.exe";
      break;
    case VER_PLATFORM_WIN32_WINDOWS:
      cmd = "command.com";
      break;
    default:
      cmd = "command.com";
      break;
    }
}

static void
run (const char *sh, const char *args, const char *file)
{
  BOOL b;
  char cmdline[_MAX_PATH];
  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  sprintf (cmdline, "%s %s %s", sh, args, file);
  memset (&pi, 0, sizeof (pi));
  memset (&si, 0, sizeof (si));
  si.cb = sizeof (si);
  si.lpTitle = (char *) "Cygwin Setup Post-Install Script";
  si.dwFlags = STARTF_USEPOSITION;

  b = CreateProcess (0, cmdline, 0, 0, 0,
		     CREATE_NEW_CONSOLE, 0, get_root_dir ().cstr_oneuse(), &si, &pi);

  if (b)
    WaitForSingleObject (pi.hProcess, INFINITE);
}

void
run_script (String const &dir, String const &fname)
{
  char *ext = strrchr (fname.cstr_oneuse(), '.');
  if (!ext)
    return;

  if (sh.size() && strcmp (ext, ".sh") == 0)
    {
      String f2 = dir + fname;
      log (LOG_PLAIN, String ("running: ") + sh + " -c " + f2);
      run (sh.cstr_oneuse(), "-c", f2.cstr_oneuse());
    }
  else if (cmd && strcmp (ext, ".bat") == 0)
    {
      String f2 = backslash (cygpath (dir + fname));
      log (LOG_PLAIN, String ("running: ") + cmd + " /c " + f2);
      run (cmd, "/c", f2.cstr_oneuse());
    }
  else
    return;

  /* if file exists then delete it otherwise just ignore no file error */
  io_stream::remove (String ("cygfile://") + dir + fname + ".done");

  io_stream::move (String ("cygfile://") + dir + fname,
                   String ("cygfile://") + dir + fname+ ".done");
}

void
try_run_script (String const &dir, String const &fname)
{
  if (io_stream::exists (String ("cygfile://")+ dir + fname + ".sh"))
    run_script (dir.cstr_oneuse(), (fname + ".sh").cstr_oneuse());
  if (io_stream::exists (String ("cygfile://")+ dir + fname + ".bat"))
    run_script (dir.cstr_oneuse(), (fname + ".bat").cstr_oneuse());
}

