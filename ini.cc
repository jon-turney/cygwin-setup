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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <process.h>

#include "ini.h"
#include "resource.h"
#include "String++.h"
#include "state.h"
#include "geturl.h"
#include "dialog.h"
#include "msg.h"
#include "log.h"
#include "version.h"
#include "mount.h"
#include "site.h"
#include "rfc1738.h"
#include "find.h"
#include "filemanip.h"

#include "io_stream.h"

#include "threebar.h"

#include "rfc1738.h"
extern ThreeBarProgressPage Progress;

unsigned int setup_timestamp = 0;
char *setup_version = 0;

extern int yyparse ();
/*extern int yydebug;*/

static char *error_buf = 0;
static int error_count = 0;

static int local_ini;

static const char *ini_filename;

static void
find_routine (char *path, unsigned int fsize)
{
  const char *setup_ini = trail (path, "\\setup.ini");

  if (setup_ini == NULL)
    return;

  unsigned pathprefix_len = setup_ini - path;

  io_stream *ini_file = io_stream::open (String ("file://") +
					 path, "rb");
  if (!ini_file)
    {
    note (NULL, IDS_SETUPINI_MISSING, (String ("file://") +
				       path).cstr_oneuse());
    return;
    }
  else
    log (LOG_BABBLE, String ("Found ini file - file://") + path);

  /* FIXME: only use most recent copy */
  setup_timestamp = 0;
  setup_version = 0;

  /* Copy leading part of path to temporary buffer and unescape it */

  char path_prefix[pathprefix_len + 1];
  strncpy (path_prefix, path, pathprefix_len);
  path_prefix[pathprefix_len] = '\0';
  String mirror = rfc1738_unescape_part (path_prefix);
  ini_init (ini_file, mirror);

  /*yydebug = 1;*/

  ini_filename = path;
  if (yyparse () || error_count > 0)
    MessageBox (0, error_buf, error_count == 1 ? "Parse Error" : "Parse Errors", 0);
  else
    local_ini++;
  *error_buf = '\0';
  error_count = 0;
}

static int
do_local_ini (HWND owner)
{
  local_ini = 0;
  find (local_dir, find_routine);
  return local_ini; 
}

static int
do_remote_ini (HWND owner)
{
  size_t ini_count = 0;
  
  for (size_t n = 1; n <= site_list.number (); n++)
    {
      io_stream *ini_file =
	get_url_to_membuf (site_list[n]->url + "/setup.ini", owner);

      if (!ini_file)
	{
	  note (owner, IDS_SETUPINI_MISSING, site_list[n]->url.cstr_oneuse());
	  continue;
	}

      /* FIXME: only use most recent copy */
      setup_timestamp = 0;
      setup_version = 0;

      ini_init (ini_file, site_list[n]->url);

      /*yydebug = 1; */

      if (yyparse () || error_count > 0)
	MessageBox (0, error_buf,
		    error_count == 1 ? "Parse Error" : "Parse Errors", 0);
      else
	{
	  /* save known-good setup.ini locally */
	  String const fp = String ("file://") + local_dir + "/" +
				   rfc1738_escape_part (site_list[n]->url) +
				   "/setup.ini";
	  io_stream::mkpath_p (PATH_TO_FILE, fp);
	  io_stream *inistream = io_stream::open (fp, "wb");
	  if (inistream && !ini_file->seek (0, IO_SEEK_SET))
	    {
	      if (io_stream::copy (ini_file, inistream))
		io_stream::remove (fp.cstr_oneuse());
	      delete ini_file;
	      delete inistream;
	    }
	  ++ini_count;
	}
    }
  return ini_count;
}

static void
do_ini_thread (HINSTANCE h, HWND owner)
{
  size_t ini_count = 0;
  if (source == IDC_SOURCE_CWD)
    ini_count = do_local_ini (owner);
  else
    ini_count = do_remote_ini (owner);

  if (ini_count == 0)
    {
      next_dialog = source == IDC_SOURCE_CWD ? IDD_S_FROM_CWD : IDD_SITE;
      return;
    }

  if (get_root_dir ().cstr_oneuse())
    {
      io_stream::mkpath_p (PATH_TO_DIR, "cygfile:///etc/setup");

      unsigned int old_timestamp = 0;
      io_stream *ots =
	io_stream::open ("cygfile:///etc/setup/timestamp", "rt");
      if (ots)
	{
	  char temp[20];
	  memset (temp, '\0', 20);
	  if (ots->read (temp, 19))
	    sscanf (temp, "%u", &old_timestamp);
	  delete ots;
	  if (old_timestamp && setup_timestamp
	      && (old_timestamp > setup_timestamp))
	    {
	      int yn = yesno (owner, IDS_OLD_SETUPINI);
	      if (yn == IDNO)
		exit_setup (1);
	    }
	}
      if (setup_timestamp)
	{
	  io_stream *nts =
	    io_stream::open ("cygfile:///etc/setup/timestamp", "wt");
	  if (nts)
	    {
	      char temp[20];
	      sprintf (temp, "%u", setup_timestamp);
	      nts->write (temp, strlen (temp));
	      delete nts;
	    }
	}
    }

  msg ("setup_version is %s, our_version is %s", setup_version ? : "(null)",
       version);
  if (setup_version)
    {
      String ini_version = canonicalize_version (setup_version);
      String our_version = canonicalize_version (version);
      // XXX useversion < operator
      if (our_version.compare (ini_version) < 0)
	note (owner, IDS_OLD_SETUP_VERSION, version, setup_version);
    }

  next_dialog = IDD_CHOOSER;
}

static DWORD WINAPI
do_ini_thread_reflector(void* p)
{
	HANDLE *context;
	context = (HANDLE*)p;

	do_ini_thread((HINSTANCE)context[0], (HWND)context[1]);

	// Tell the progress page that we're done downloading
	Progress.PostMessage(WM_APP_SETUP_INI_DOWNLOAD_COMPLETE, 0, next_dialog);

	ExitThread(0);
}

static HANDLE context[2];

void
do_ini (HINSTANCE h, HWND owner)
{
  context[0] = h;
  context[1] = owner;
	
  DWORD threadID;	
  CreateThread (NULL, 0, do_ini_thread_reflector, context, 0, &threadID);
}

extern int yylineno;
extern int yybol ();

extern "C" int
yyerror (char *s, ...)
{
  char buf[MAX_PATH + 1000];
  int len;
  sprintf (buf, "%s line %d: ", ini_filename, yylineno - yybol ());
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
  /* TODO: is return 0 correct? */
  return 0;
}

extern "C" int fprintf (FILE * f, const char *s, ...);

static char stderrbuf[1000];

int
fprintf (FILE * f, const char *fmt, ...)
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
	  /*OutputDebugString (stderrbuf); */
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
