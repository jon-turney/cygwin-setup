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

#include "ini.h"

#include "csu_util/rfc1738.h"
#include "csu_util/version_compare.h"

#include "setup_version.h"
#include "win32.h"
#include "LogSingleton.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <process.h>

#include "resource.h"
#include "state.h"
#include "geturl.h"
#include "dialog.h"
#include "msg.h"
#include "mount.h"
#include "site.h"
#include "find.h"
#include "IniParseFindVisitor.h"
#include "IniParseFeedback.h"

#include "io_stream.h"

#include "threebar.h"

#include "IniDBBuilderPackage.h"
#include "compress.h"
#include "Exception.h"
  
extern ThreeBarProgressPage Progress;

unsigned int setup_timestamp = 0;
std::string ini_setup_version;

extern int yyparse ();
/*extern int yydebug;*/

static char *error_buf = 0;
static int error_count = 0;

static const char *ini_filename;

class GuiParseFeedback : public IniParseFeedback
{
public:
  GuiParseFeedback () : lastpct (0) 
    {
      Progress.SetText2 ("");
      Progress.SetText3 ("");
      Progress.SetText4 ("Progress:");
    }
  virtual void progress(unsigned long const pos, unsigned long const max)
    {
      if (!max)
	/* length not known or eof */
	return;
      if (lastpct == 100)
	/* rounding down should mean this only ever fires once */
	lastpct = 0;
      if (pos * 100 / max > lastpct)
	{
	  lastpct = pos * 100 / max;
	  log (LOG_BABBLE) << lastpct << "% (" << pos << " of " << max
            << " bytes of ini file read)" << endLog;
	}
      Progress.SetBar1(pos, max);
    }
  virtual void iniName (const std::string& name)
    {
      Progress.SetText1 (("Parsing ini file \"" + name + "\"").c_str());
    }
  virtual void babble(const std::string& message)const
    {
      log (LOG_BABBLE) << message << endLog;
    }
  virtual void warning (const std::string& message)const
    {
      MessageBox (0, message.c_str(), "Warning", 0);
    }
  virtual void error(const std::string& message)const
    {
      MessageBox (0, message.c_str(), "Error parsing", 0);
    }
  virtual ~ GuiParseFeedback ()
    {
      Progress.SetText4("Package:");
    }
private:
  unsigned int lastpct;
};

static int
do_local_ini (HWND owner)
{
  GuiParseFeedback myFeedback;
  IniDBBuilderPackage findBuilder(myFeedback);
  IniParseFindVisitor myVisitor (findBuilder, local_dir, myFeedback);
  Find (local_dir).accept(myVisitor);
  setup_timestamp = myVisitor.timeStamp();
  ini_setup_version = myVisitor.version();
  return myVisitor.iniCount(); 
}

static int
do_remote_ini (HWND owner)
{
  size_t ini_count = 0;
  GuiParseFeedback myFeedback;
  IniDBBuilderPackage aBuilder(myFeedback);

  for (SiteList::const_iterator n = site_list.begin();
       n != site_list.end(); ++n)
    {
      io_stream *compressed_ini_file =
	get_url_to_membuf (n->url + "/setup.bz2", owner);
      io_stream *ini_file = 0;
      if (!compressed_ini_file)
	ini_file = get_url_to_membuf (n->url + "/setup.ini", owner);
      else
	{
	  ini_file = compress::decompress (compressed_ini_file);
	}

      if (!ini_file)
	{
	  note (owner, IDS_SETUPINI_MISSING, n->url.c_str());
	  continue;
	}
 
      if (compressed_ini_file)
        myFeedback.iniName (n->url + "/setup.bz2");
      else
        myFeedback.iniName (n->url + "/setup.ini");

      aBuilder.parse_mirror = n->url;
      ini_init (ini_file, &aBuilder, myFeedback);

      /*yydebug = 1; */

      if (yyparse () || error_count > 0)
	MessageBox (0, error_buf,
		    error_count == 1 ? "Parse Error" : "Parse Errors", 0);
      else
	{
	  /* save known-good setup.ini locally */
	  const std::string fp = "file://" + local_dir + "/" +
				   rfc1738_escape_part (n->url) +
				   "/setup.ini";
	  io_stream::mkpath_p (PATH_TO_FILE, fp);
	  io_stream *inistream = io_stream::open (fp, "wb");
	  if (inistream)
	    {
	      if (compressed_ini_file)
		{
		  delete ini_file;
		  compressed_ini_file->seek (0, IO_SEEK_SET);
		  ini_file = compress::decompress (compressed_ini_file);
		}
	      else
   		ini_file->seek (0, IO_SEEK_SET);
	      if (io_stream::copy (ini_file, inistream))
		io_stream::remove (fp);
	      delete inistream;
	    }
	  ++ini_count;
	}
      if (aBuilder.timestamp > setup_timestamp)
	{
	  setup_timestamp = aBuilder.timestamp;
	  ini_setup_version = aBuilder.version;
	}
      delete ini_file;
      delete compressed_ini_file;
    }
  return ini_count;
}

static bool
do_ini_thread (HINSTANCE h, HWND owner)
{
  size_t ini_count = 0;
  if (source == IDC_SOURCE_CWD)
    ini_count = do_local_ini (owner);
  else
    ini_count = do_remote_ini (owner);

  if (ini_count == 0)
    return false;

  if (get_root_dir ().c_str())
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
		LogSingleton::GetInstance().exit (1);
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

  msg (".ini setup_version is %s, our setup_version is %s", ini_setup_version.size() ? 
       ini_setup_version.c_str() : "(null)",
       setup_version);
  if (ini_setup_version.size())
    {
      if (version_compare(setup_version, ini_setup_version) < 0)
	note (owner, IDS_OLD_SETUP_VERSION, setup_version,
              ini_setup_version.c_str());
    }

  return true;
}

static DWORD WINAPI
do_ini_thread_reflector(void* p)
{
  HANDLE *context;
  context = (HANDLE*)p;

  try
  {
    bool succeeded = do_ini_thread((HINSTANCE)context[0], (HWND)context[1]);

    // Tell the progress page that we're done downloading
    Progress.PostMessage(WM_APP_SETUP_INI_DOWNLOAD_COMPLETE, 0, succeeded);
  }
  TOPLEVEL_CATCH("ini");

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

extern int
yyerror (const std::string& s)
{
  char buf[MAX_PATH + 1000];
  int len;
  sprintf (buf, "%s line %d: ", ini_filename, yylineno - yybol ());
  sprintf (buf + strlen (buf), s.c_str());
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
      rv = vsnprintf (buf, 1000, fmt, args);
      /* todo check here for overflows too */
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
