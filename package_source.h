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

class packagesource
{
public:
  /* The canonical name - the complete path to the source file */
  virtual const char *Canonical () = 0;
  /* The basename - without extention */
  virtual const char *Base () = 0;
  /* The basename - with extention */
  virtual const char *Filename () = 0;
  /* The name from the installation cache - may be the same as Canonical */
  virtual const char *CachedName () = 0;

    virtual ~ packagesource ()
  {
  };

private:
};

#endif /* _PACKAGE_SOURCE_H_ */
