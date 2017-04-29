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
  packagesource ():size (0), canonical (0), base (0), filename (0), cached ()
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
    return canonical;
  };
  /* The basename - without extention 
   * i.e. package-1
   */
  const char *Base () const
  {
    return base;
  };
  /* The basename - with extention 
   * i.e. package-1.tar.bz2
   */
  const char *Filename () const
  {
    return filename;
  };
  /* what is the cached filename, to prevent directory scanning during install */
  char const *Cached () const
  {
    /* Pointer-coerce-to-boolean is used by many callers. */
    if (cached.empty())
      return NULL;
    return cached.c_str();
  };
  /* sets the canonical path, and parses and creates base and filename */
  void set_canonical (char const *);
  void set_cached (const std::string& );
  unsigned char sha512sum[SHA512_DIGEST_LENGTH];
  bool sha512_isSet;
  MD5Sum md5;
  typedef std::vector <site> sitestype;
  sitestype sites;

  ~packagesource ()
  {
    if (canonical)
      delete []canonical;
    if (base)
      delete []base;
    if (filename)
      delete []filename;
  };

private:
  char *canonical;
  char *base;
  char *filename;
  std::string cached;
};

#endif /* SETUP_PACKAGE_SOURCE_H */
