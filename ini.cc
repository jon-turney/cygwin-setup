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

/* The purpose of this file is to get and parse the setup.ini file
   from the mirror site.  A few support routines for the bison and
   flex parsers are provided also.  We check to see if this setup.ini
   is older than the one we used last time, and if so, warn the user. */

static char *cvsid = "\n%%% $Id$\n";

#include "win32.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "ini.h"
#include "resource.h"
#include "concat.h"
#include "state.h"
#include "geturl.h"
#include "dialog.h"
#include "msg.h"
#include "mkdir.h"
#include "log.h"

unsigned int setup_timestamp = 0;

extern "C" int yyparse ();
/*extern int yydebug;*/

static char *error_buf = 0;
static int error_count = 0;

void
do_ini (HINSTANCE h)
{
  char *ini_file = get_url_to_string (concat (MIRROR_SITE, "/setup.ini", 0));
  dismiss_url_status_dialog ();

  if (!ini_file)
    {
      note (IDS_SETUPINI_MISSING, MIRROR_SITE);
      next_dialog = IDD_SITE;
      return;
    }

  ini_init (ini_file);

  setup_timestamp = 0;
  /*yydebug = 1;*/

  if (yyparse () || error_count > 0)
    {
      if (error_count == 1)
	MessageBox (0, error_buf, "Parse Error", 0);
      else
	MessageBox (0, error_buf, "Parse Errors", 0);
    }

  if (root_dir)
    {
      mkdir_p (1, concat (root_dir, "/etc/setup", 0));

      unsigned int old_timestamp = 0;
      FILE *ots = fopen (concat (root_dir, "/etc/setup/timestamp", 0), "rt");
      if (ots)
	{
	  fscanf (ots, "%u", &old_timestamp);
	  fclose (ots);
	  if (old_timestamp && setup_timestamp
	      && (old_timestamp > setup_timestamp))
	    {
	      int yn = yesno (IDS_OLD_SETUPINI);
	      if (yn == IDNO)
		exit_setup (1);
	    }
	}
      if (setup_timestamp)
	{
	  FILE *nts = fopen (concat (root_dir, "/etc/setup/timestamp", 0), "wt");
	  if (nts)
	    {
	      fprintf (nts, "%u", setup_timestamp);
	      fclose (nts);
	    }
	}
    }

  next_dialog = IDD_CHOOSE;
}

extern int yylineno;

extern "C" int yyerror (char *s, ...)
{
  char buf[1000];
  int len;
  sprintf (buf, "setup.ini line %d: ", yylineno);
  va_list args;
  va_start (args, s);
  vsprintf (buf + strlen (buf), s, args);
  OutputDebugString (buf);
  if (error_buf)
    {
      strcat (error_buf, "\n");
      len = strlen (error_buf) + strlen (buf) + 5;
      error_buf = (char *) realloc (error_buf, len);
      strcat (error_buf, buf);
    }
  else
    {
      len = strlen (buf) + 5;
      error_buf = (char *) malloc (len);
      strcpy (error_buf, buf);
    }
  error_count++;
}

extern "C" int fprintf (FILE *f, const char *s, ...);

static char stderrbuf[1000];

int
fprintf (FILE *f, const char *fmt, ...)
{
  char buf[1000];
  int rv;
  va_list args;
  va_start (args, fmt);
  if (f == stderr)
    {
      rv = vsprintf (buf, fmt, args);
      strcat (stderrbuf, buf);
      if (char *nl = strchr (stderrbuf, '\n'))
	{
	  *nl = 0;
	  /*OutputDebugString (stderrbuf);*/
	  MessageBox (0, buf, "Cygwin Setup", 0);
	  stderrbuf[0] = 0;
	}
      
    }
  else
    {
      rv = vfprintf (f, fmt, args);
    }
  return rv;
}
