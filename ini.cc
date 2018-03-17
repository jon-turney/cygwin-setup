/*
 * Copyright (c) 2000,2007 Red Hat, Inc.
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

#include "ini.h"

#include "csu_util/rfc1738.h"
#include "csu_util/version_compare.h"

#include "setup_version.h"
#include "win32.h"
#include "LogFile.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <process.h>

#include "resource.h"
#include "state.h"
#include "geturl.h"
#include "dialog.h"
#include "mount.h"
#include "site.h"
#include "find.h"
#include "IniParseFeedback.h"

#include "io_stream.h"
#include "io_stream_memory.h"

#include "threebar.h"

#include "getopt++/BoolOption.h"
#include "IniDBBuilderPackage.h"
#include "compress.h"
#include "Exception.h"
#include "crypto.h"
#include "package_db.h"

extern ThreeBarProgressPage Progress;

unsigned int setup_timestamp = 0;
std::string ini_setup_version;
// TODO: use C++11x initializer lists instead and drop the literal array
IniList setup_ext_list (setup_exts,
			setup_exts + (sizeof(setup_exts) / sizeof(*setup_exts)));

static BoolOption NoVerifyOption (false, 'X', "no-verify", "Don't verify setup.ini signatures");

extern int yyparse ();

/*extern int yydebug;*/

class GuiParseFeedback : public IniParseFeedback
{
public:
  GuiParseFeedback () : lastpct (0)
    {
      Progress.SetText2 ("");
      Progress.SetText3 ("");
      Progress.SetText4 ("Progress:");
    }
  virtual void progress (unsigned long const pos, unsigned long const max)
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
	  /* Log (LOG_BABBLE) << lastpct << "% (" << pos << " of " << max
	    << " bytes of ini file read)" << endLog; */
	}
      Progress.SetBar1 (pos, max);

      static char buf[100];
      sprintf (buf, "%d %%  (%ldk/%ldk)", lastpct, pos/1000, max/1000);
      Progress.SetText3 (buf);
    }
  virtual void iniName (const std::string& name)
    {
      Progress.SetText1 ("Parsing...");
      Progress.SetText2 (name.c_str ());
      Progress.SetText3 ("");
    }
  virtual void babble (const std::string& message)const
    {
      Log (LOG_BABBLE) << message << endLog;
    }
  virtual void warning (const std::string& message)const
    {
      mbox (Progress.GetHWND(), message.c_str (), "Warning", 0);
    }
  virtual void error (const std::string& message)const
    {
      mbox (Progress.GetHWND(), message.c_str (), "Parse Errors", 0);
    }
  virtual ~ GuiParseFeedback ()
    {
      Progress.SetText4 ("Package:");
    }
private:
  unsigned int lastpct;
};

static io_stream*
decompress_ini (io_stream *ini_file)
{
  // Replace the current compressed setup stream with its decompressed
  // version.  Which decompressor to use is determined by file magic.
  io_stream *compressed_stream = compress::decompress (ini_file);
  if (!compressed_stream)
    {
      /* This isn't a known compression format or an uncompressed file
	 stream.  Pass it on in case it was uncompressed, it will
	 generate a parser error if it was some unknown format. */
      delete compressed_stream;
    }
  else
    {
      /* Decompress the entire file in memory.  This has the advantage
	 that input_stream->get_size () will work during parsing and
	 we'll have an accurate status bar.  Also, we can't seek
	 compressed streams, so when we write out a local cached copy
	 of the .ini file below, we'd otherwise have to delete this
	 stream and uncompress it again from the start, which is
	 wasteful.  The current uncompressed size of the setup.ini
	 file as of 2015 is about 5 MiB, so this is not a great deal
	 of memory.  */
      io_stream *uncompressed = new io_stream_memory ();
      /* Note that the decompress io_stream now "owns" the underlying
	 compressed io_stream instance, so it need not be deleted
	 explicitly. */
      if ((io_stream::copy (compressed_stream, uncompressed) != 0) ||
	  (compressed_stream->error () != 0))
	{
	  /* There was a problem decompressing compressed_stream.  */
	  Log (LOG_PLAIN) <<
	    "Warning: Error code " << compressed_stream->error () <<
	    " occurred while uncompressing " << current_ini_name <<
	    " - possibly truncated or corrupt file. " << endLog;
	  delete uncompressed;
	  ini_file = NULL;
	}
      else
	{
	  ini_file = uncompressed;
	  ini_file->seek (0, IO_SEEK_SET);
	}
    }
  return ini_file;
}

static io_stream*
check_ini_sig (io_stream* ini_file, io_stream* ini_sig_file,
	       bool& sig_fail, const char* site, const char* sig_name, HWND owner)
{
  /* Unless the NoVerifyOption is set, check the signature for the
     current setup and record the result.  On a failed signature check
     the streams are invalidated so even if we tried to read in the
     setup anyway there's be nothing to parse. */
  if (!NoVerifyOption && ini_file)
    {
      if (!ini_sig_file) {
	// don't complain if the user installs from localdir and no
	// signature file is present
	// TODO: download the ini + signature file instead
	if (casecompare (site, "localdir"))
	  {
	    note (owner, IDS_SETUPINI_MISSING, sig_name, site);
	    delete ini_file;
	    ini_file = NULL;
	    sig_fail = true;
	  }
      }
      else if (!verify_ini_file_sig (ini_file, ini_sig_file, owner))
	{
	  note (owner, IDS_SIG_INVALID, sig_name, site);
	  delete ini_sig_file;
	  ini_sig_file = NULL;
	  delete ini_file;
	  ini_file = NULL;
	  sig_fail = true;
	}
    }
  return ini_file;
}

static bool
do_local_ini (HWND owner)
{
  bool ini_error = false;
  io_stream *ini_file, *ini_sig_file;
  // iterate over all setup files found in do_from_local_dir
  for (IniList::const_iterator n = found_ini_list.begin ();
       n != found_ini_list.end (); ++n)
    {
      GuiParseFeedback myFeedback;
      IniDBBuilderPackage aBuilder (myFeedback);
      bool sig_fail = false;
      std::string current_ini_ext, current_ini_name, current_ini_sig_name;

      current_ini_name = *n;
      current_ini_sig_name = current_ini_name + ".sig";
      current_ini_ext = current_ini_name.substr (current_ini_name.rfind (".") + 1);
      ini_sig_file = io_stream::open ("file://" + current_ini_sig_name, "rb", 0);
      ini_file = io_stream::open ("file://" + current_ini_name, "rb", 0);
      ini_file = check_ini_sig (ini_file, ini_sig_file, sig_fail,
				"localdir", current_ini_sig_name.c_str (), owner);
      if (ini_file)
	ini_file = decompress_ini (ini_file);
      if (!ini_file || sig_fail)
	{
	  // no setup found or signature invalid
	  note (owner, IDS_SETUPINI_MISSING, SetupBaseName.c_str (),
		"localdir");
	  ini_error = true;
	}
      else
	{
	  // grok information from setup
	  myFeedback.babble ("Found ini file - " + current_ini_name);
	  myFeedback.iniName (current_ini_name);
	  int ldl = local_dir.length () + 1;
	  int cap = current_ini_name.rfind ("/" + SetupArch);
	  aBuilder.parse_mirror =
	    rfc1738_unescape (current_ini_name.substr (ldl, cap - ldl));
	  ini_init (ini_file, &aBuilder, myFeedback);

	  if (yyparse () || yyerror_count > 0)
	    {
	      myFeedback.error (yyerror_messages);
	      ini_error = true;
	    }

	  if (aBuilder.timestamp > setup_timestamp)
	    {
	      setup_timestamp = aBuilder.timestamp;
	      ini_setup_version = aBuilder.version;
	    }
	  delete ini_file;
	  ini_file = NULL;
	}
    }
  return ini_error;
}

static bool
do_remote_ini (HWND owner)
{
  bool ini_error = false;
  io_stream *ini_file = NULL, *ini_sig_file;

  /* FIXME: Get rid of this io_stream pointer travesty.  The need to
     explicitly delete these things is ridiculous. */

  // iterate over all sites
  for (SiteList::const_iterator n = site_list.begin ();
       n != site_list.end (); ++n)
    {
      GuiParseFeedback myFeedback;
      IniDBBuilderPackage aBuilder (myFeedback);
      bool sig_fail = false;
      std::string current_ini_ext, current_ini_name, current_ini_sig_name;
      // iterate over known extensions for setup
      for (IniList::const_iterator ext = setup_ext_list.begin ();
	   ext != setup_ext_list.end ();
	   ext++)
	{
	  current_ini_ext = *ext;
	  current_ini_name = n->url + SetupIniDir + SetupBaseName + "." + current_ini_ext;
	  current_ini_sig_name = current_ini_name + ".sig";
	  ini_sig_file = get_url_to_membuf (current_ini_sig_name, owner);
	  ini_file = get_url_to_membuf (current_ini_name, owner);
	  ini_file = check_ini_sig (ini_file, ini_sig_file, sig_fail,
				    n->url.c_str (), current_ini_sig_name.c_str (), owner);
	  // stop searching as soon as we find a setup file
	  if (ini_file)
	    break;
	}
      if (ini_file)
	ini_file = decompress_ini (ini_file);
      if (!ini_file || sig_fail)
	{
	  // no setup found or signature invalid
	  note (owner, IDS_SETUPINI_MISSING, SetupBaseName.c_str (), n->url.c_str ());
	  ini_error = true;
	}
      else
	{
	  // grok information from setup
	  myFeedback.iniName (current_ini_name);
	  aBuilder.parse_mirror = n->url;
	  ini_init (ini_file, &aBuilder, myFeedback);

	  if (yyparse () || yyerror_count > 0)
	    {
	      myFeedback.error (yyerror_messages);
	      ini_error = true;
	    }
	  else
	    {
	      /* save known-good setup.ini locally */
	      const std::string fp = "file://" + local_dir + "/" +
				      rfc1738_escape_part (n->url) +
				      "/" + SetupIniDir + SetupBaseName + ".ini";
	      io_stream::mkpath_p (PATH_TO_FILE, fp, 0);
	      if (io_stream *out = io_stream::open (fp, "wb", 0))
		{
		  ini_file->seek (0, IO_SEEK_SET);
		  if (io_stream::copy (ini_file, out) != 0)
		    io_stream::remove (fp);
		  delete out;
		}
	    }
	  if (aBuilder.timestamp > setup_timestamp)
	    {
	      setup_timestamp = aBuilder.timestamp;
	      ini_setup_version = aBuilder.version;
	    }
	  delete ini_file;
	  ini_file = NULL;
	}
    }
  return ini_error;
}

static bool
do_ini_thread (HINSTANCE h, HWND owner)
{
  packagedb db;
  db.init();

  bool ini_error = true;

  if (source == IDC_SOURCE_LOCALDIR)
    ini_error = do_local_ini (owner);
  else
    ini_error = do_remote_ini (owner);

  if (ini_error)
    return false;

  if (get_root_dir ().c_str ())
    {
      io_stream::mkpath_p (PATH_TO_DIR, "cygfile:///etc/setup", 0755);

      unsigned int old_timestamp = 0;
      io_stream *ots =
	io_stream::open ("cygfile:///etc/setup/timestamp", "rt", 0);
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
		Logger ().exit (1);
	    }
	}
      if (setup_timestamp)
	{
	  io_stream *nts =
	    io_stream::open ("cygfile:///etc/setup/timestamp", "wt", 0);
	  if (nts)
	    {
	      char temp[20];
	      sprintf (temp, "%u", setup_timestamp);
	      nts->write (temp, strlen (temp));
	      delete nts;
	    }
	}
    }

  LogBabblePrintf (".ini setup_version is %s, our setup_version is %s", ini_setup_version.size () ?
       ini_setup_version.c_str () : "(null)",
       setup_version);
  if (ini_setup_version.size ())
    {
      if (version_compare (setup_version, ini_setup_version) < 0)
	note (owner, IDS_OLD_SETUP_VERSION, setup_version,
	      ini_setup_version.c_str ());
    }

  return true;
}

static DWORD WINAPI
do_ini_thread_reflector (void* p)
{
  HANDLE *context;
  context = (HANDLE*)p;

  try
  {
    bool succeeded = do_ini_thread ((HINSTANCE)context[0], (HWND)context[1]);

    // Tell the progress page that we're done downloading
    Progress.PostMessageNow (WM_APP_SETUP_INI_DOWNLOAD_COMPLETE, 0, succeeded);
  }
  TOPLEVEL_CATCH ((HWND) context[1], "ini");

  ExitThread (0);
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
