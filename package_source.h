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

#ifndef SETUP_PACKAGE_SOURCE_H
#define SETUP_PACKAGE_SOURCE_H

/* this is the parent class for all package source (not source code - installation
 * source as in http/ftp/disk file) operations.
 */

#include "sha2.h"
#include "strings.h"
#include "String++.h"
#include "csu_util/MD5Sum.h"
#include <vector>

class site
{
public:
  site (const std::string& newkey);
  ~site () {}
  std::string key;
  bool operator == (site const &rhs)
    {
      return casecompare(key, rhs.key) == 0;
    }
};

class packagesource
{
public:
  packagesource ():size (0), canonical (), shortname (), cached (), validated (false)
  {
    memset (sha512sum, 0, sizeof sha512sum);
    sha512_isSet = false;
  };
  /* how big is the source file */
  size_t size;
  /* The canonical name - the complete path to the source file
   * i.e. foo/bar/package-1.tar.bz2
   */
  const char *Canonical () const
  {
    if (!canonical.empty())
      return canonical.c_str();

    return NULL;
  };
  /* What is the cached filename, to prevent directory scanning during
     install?  Except in mirror mode, we never set this without
     checking the size.  The more expensive hash checking is reserved
     for verifying the integrity of downloaded files and sources of
     packages about to be installed.  Set the 'validated' flag to
     avoid checking the hash twice.  */
  char const *Cached () const
  {
    /* Pointer-coerce-to-boolean is used by many callers. */
    if (cached.empty())
      return NULL;
    return cached.c_str();
  };
  /* sets the canonical path */
  void set_canonical (char const *);
  void set_cached (const std::string& );
  unsigned char sha512sum[SHA512_DIGEST_LENGTH];
  bool sha512_isSet;
  MD5Sum md5;
  /* The next two functions throw exceptions on failure.  */
  void check_size_and_cache (const std::string fullname);
  void check_hash ();
  typedef std::vector <site> sitestype;
  sitestype sites;

private:
  std::string canonical;
  /* For progress reporting.  */
  std::string shortname;
  std::string cached;
  bool validated;
  void check_sha512 (const std::string fullname) const;
  void check_md5 (const std::string fullname) const;
};

#endif /* SETUP_PACKAGE_SOURCE_H */
