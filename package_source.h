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

#ifndef _PACKAGE_SOURCE_H_
#define _PACKAGE_SOURCE_H_

/* required to parse this file */
#include "strings.h"
#include "String++.h"
#include "MD5++.h"
#include <vector>

/* standard binary package metadata:
 * Name (ie mutt
 * Vendor Version (ie 2.5.1)
 * Package Version (ie 16)
 * Stability 
 * Files 
 */

/* For non installed files, this class can be populated via information about
 * what is available on the net, or by parsing a specific package file.
 * for installed packages, this class should represent what is currently installed,
 * - updated by what net metadata has about it.
 * i.e. the stability of this version will change simply because the net mirrors
 * now consider it old.
 */

class site
{
public:
  site (String const &newkey);
  ~site () {}
  String key;
  bool operator == (site const &rhs)
    {
      return key.casecompare (rhs.key) == 0;
    }
};

class packagesource
{
public:
  packagesource ():size (0), canonical (0), base (0), filename (0), cached (),
   _installedSize (0)
  {
  };
  /* how big is the source file */
  size_t size;
  /* how much space do we need to install this ? */
  virtual unsigned long installedSize ()
    {
      return _installedSize;
    }
  virtual void setInstalledSize (unsigned long size)
    {
      _installedSize = size;
    }
  /* The canonical name - the complete path to the source file 
   * i.e. foo/bar/package-1.tar.bz2
   */
  virtual const char *Canonical ()
  {
    return canonical;
  };
  /* The basename - without extention 
   * i.e. package-1
   */
  virtual const char *Base ()
  {
    return base;
  };
  /* The basename - with extention 
   * i.e. package-1.tar.bz2
   */
  virtual const char *Filename ()
  {
    return filename;
  };
  /* what is the cached filename, to prevent directory scanning during install */
  virtual char const *Cached () const
  {
    return cached.cstr_oneuse();
  };
  /* sets the canonical path, and parses and creates base and filename */
  virtual void set_canonical (char const *);
  virtual void set_cached (String const &);
  class md5 md5;
  typedef vector <site> sitestype;
  sitestype sites;

  virtual ~ packagesource ()
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
  String cached;
  unsigned long _installedSize;
};

#endif /* _PACKAGE_SOURCE_H_ */
