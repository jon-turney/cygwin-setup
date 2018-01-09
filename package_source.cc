/*
 * Copyright (c) 2001, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

/* this is the parent class for all package source (not source code - installation
 * source as in http/ftp/disk file) operations.
 */

#include "package_source.h"
#include "sha2.h"
#include "csu_util/MD5Sum.h"
#include "LogFile.h"
#include "threebar.h"
#include "Exception.h"
#include "filemanip.h"
#include "io_stream.h"

extern ThreeBarProgressPage Progress;

site::site (const std::string& newkey) : key(newkey)
{
}

void
packagesource::set_canonical (char const *fn)
{
  canonical = fn;
  size_t found = canonical.find_last_of ("/");
  shortname = canonical.substr (found + 1);
}

void
packagesource::set_cached (const std::string& fp)
{
  cached = fp;
}

void
packagesource::check_size_and_cache (const std::string fullname)
{
  DWORD fnsize = get_file_size (fullname);
  if (fnsize != size)
    {
      Log (LOG_BABBLE) << "INVALID PACKAGE: " << fullname
		       << " - Size mismatch: Ini-file: " << size
		       << " != On-disk: " << fnsize << endLog;
      throw new Exception (TOSTRING(__LINE__) " " __FILE__,
			   "Size mismatch for " + fullname,
			   APPERR_CORRUPT_PACKAGE);
    }
  cached = fullname;
  validated = false;
}

void
packagesource::check_hash ()
{
  if (validated || cached.empty ())
    return;

  if (sha512_isSet)
    {
      check_sha512 (cached);
      validated = true;
    }
  else if (md5.isSet())
    {
      check_md5 (cached);
      validated = true;
    }
  else
    Log (LOG_BABBLE) << "No checksum recorded for " << cached
		     << ", cannot determine integrity of package!"
		     << endLog;
}

static char *
sha512_str (const unsigned char *in, char *buf)
{
  char *bp = buf;
  for (int i = 0; i < SHA512_DIGEST_LENGTH; ++i)
    bp += sprintf (bp, "%02x", in[i]);
  *bp = '\0';
  return buf;
}

void
packagesource::check_sha512 (const std::string fullname) const
{
  io_stream *thefile = io_stream::open (fullname, "rb", 0);
  if (!thefile)
    throw new Exception (TOSTRING (__LINE__) " " __FILE__,
			 std::string ("IO Error opening ") + fullname,
			 APPERR_IO_ERROR);
  SHA2_CTX ctx;
  unsigned char sha512result[SHA512_DIGEST_LENGTH];
  char ini_sum[SHA512_DIGEST_STRING_LENGTH],
       disk_sum[SHA512_DIGEST_STRING_LENGTH];

  SHA512Init (&ctx);

  Log (LOG_BABBLE) << "Checking SHA512 for " << fullname << endLog;

  Progress.SetText1 (("Checking SHA512 for " + shortname).c_str ());
  Progress.SetText4 ("Progress:");
  Progress.SetBar1 (0);

  unsigned char buffer[64 * 1024];
  ssize_t count;
  while ((count = thefile->read (buffer, sizeof (buffer))) > 0)
  {
    SHA512Update (&ctx, buffer, count);
    Progress.SetBar1 (thefile->tell (), thefile->get_size ());
  }
  delete thefile;
  if (count < 0)
    throw new Exception (TOSTRING(__LINE__) " " __FILE__,
			 "IO Error reading " + fullname,
			 APPERR_IO_ERROR);

  SHA512Final (sha512result, &ctx);

  if (memcmp (sha512sum, sha512result, sizeof sha512result))
    {
      Log (LOG_BABBLE) << "INVALID PACKAGE: " << fullname
		       << " - SHA512 mismatch: Ini-file: "
		       << sha512_str (sha512sum, ini_sum)
		       << " != On-disk: "
		       << sha512_str (sha512result, disk_sum)
		       << endLog;
      throw new Exception (TOSTRING(__LINE__) " " __FILE__,
			   "SHA512 failure for " + fullname,
			   APPERR_CORRUPT_PACKAGE);
    }

  Log (LOG_BABBLE) << "SHA512 verified OK: " << fullname << " "
    <<  sha512_str (sha512sum, ini_sum) << endLog;
}

void
packagesource::check_md5 (const std::string fullname) const
{
  io_stream *thefile = io_stream::open (fullname, "rb", 0);
  if (!thefile)
    throw new Exception (TOSTRING (__LINE__) " " __FILE__,
			 std::string ("IO Error opening ") + fullname,
			 APPERR_IO_ERROR);
  MD5Sum tempMD5;
  tempMD5.begin ();

  Log (LOG_BABBLE) << "Checking MD5 for " << fullname << endLog;

  Progress.SetText1 (("Checking MD5 for " + shortname).c_str ());
  Progress.SetText4 ("Progress:");
  Progress.SetBar1 (0);

  unsigned char buffer[64 * 1024];
  ssize_t count;
  while ((count = thefile->read (buffer, sizeof (buffer))) > 0)
    {
      tempMD5.append (buffer, count);
      Progress.SetBar1 (thefile->tell (), thefile->get_size ());
    }
  delete thefile;
  if (count < 0)
    throw new Exception (TOSTRING(__LINE__) " " __FILE__,
			 "IO Error reading " + fullname,
			 APPERR_IO_ERROR);

  tempMD5.finish ();

  if (md5 != tempMD5)
    {
      Log (LOG_BABBLE) << "INVALID PACKAGE: " << fullname
	<< " - MD5 mismatch: Ini-file: " << md5.str()
	<< " != On-disk: " << tempMD5.str() << endLog;
      throw new Exception (TOSTRING(__LINE__) " " __FILE__,
			   "MD5 failure for " + fullname,
			   APPERR_CORRUPT_PACKAGE);
    }

  Log (LOG_BABBLE) << "MD5 verified OK: " << fullname << " "
    << md5.str() << endLog;
}
