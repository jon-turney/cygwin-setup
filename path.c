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
 * Written by Ron Parker <parkerrd@hotmail.com>
 *
 */

#include <windows.h>
#include <wininet.h>
#include <assert.h>
#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <shellapi.h>
#include <shlguid.h>
#include <shlobj.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "setup.h"
#include "strarry.h"
#include "zlib/zlib.h"

static FILE *cygin, *cygout;
static HANDLE hcygpath;

static void
kill_cygpath (int sig)
{
  TerminateProcess (hcygpath, 0);
  exit (1);
}

static void
exit_cygpath (void)
{
  fclose (cygin);
  fclose (cygout);
  Sleep (0);
  TerminateProcess (hcygpath, 0);
}

static int
cygpath_pipe ()
{
  int hpipein[2] = {-1, -1};
  int hpipeout[2] = {-1, -1};
  char buffer[256];
  
  HANDLE hin, hout;
  if (_pipe (hpipein, 256, O_TEXT) == -1)
    return 0;
  if (_pipe (hpipeout, 256, O_TEXT) == -1)
    return 0;

  hin = (HANDLE) _get_osfhandle (hpipein[1]);
  hout = (HANDLE) _get_osfhandle (hpipeout[0]);
  sprintf (buffer, "cygpath -a -o -f - -c %lx", (unsigned long) _get_osfhandle (hpipeout[1]));
  hcygpath = (HANDLE) xcreate_process (0, hout, hin, hin, buffer);
  if (!hcygpath)
    return 0;
  atexit (exit_cygpath);
  signal (SIGINT, kill_cygpath);
  _close (hpipein[1]);
  _close (hpipeout[0]);
  cygin = fdopen (hpipein[0], "rt");
  cygout = fdopen (hpipeout[1], "wt");
  setbuf (cygout, NULL);
  return 1;
}

char *
pathcvt (char target, const char *path)
{
  char buffer[1024];
  char *retval;

  if (!cygin && !cygpath_pipe ())
    return NULL;	/* FIXME - error */

  fprintf (cygout, "-%c %s\n", target, path);
  retval = fgets (buffer, sizeof (buffer), cygin);
  if (retval)
    {
      char *p = strchr (buffer, '\n');
      if (p != NULL)
	*p = '\0';
      retval = xstrdup (buffer);
    }

  /* If there is an error try using the original style path anyway. */
  return retval ? retval : xstrdup (path);
}

char *
dtoupath (const char *path)
{
  char *retval = pathcvt ('u', path);
  size_t len = strlen (retval);
  if (len > 2 && retval[len - 1] == '/')	/* Trim the trailing slash
						   off of a nonroot path. */
    retval[len - 1] = '\0';

  return retval;
}

char *
utodpath (const char *path)
{
  char *retval = pathcvt ('w', path);
  size_t len = strlen (retval);
  if (len > 3 && retval[len - 1] == '\\')	/* Trim the trailing slash
						   off of a nonroot path. */
    retval[len - 1] = '\0';

  return retval;
}

char *
pathcat (const char *arg1, const char *arg2)
{
  char path[_MAX_PATH];
  size_t len;

  assert (!strchr (arg1, '/'));
  strcpy (path, arg1);

  /* Remove any trailing slash */
  len = strlen (path);
  if (path[--len] == '\\')
    path[len] = '\0';

  strcat (path, "\\");

  if (*arg2 == '\\')
    ++arg2;

  strcat (path, arg2);

  return xstrdup (path);
}
