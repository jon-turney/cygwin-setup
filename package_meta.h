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

#ifndef _PACKAGE_META_H_
#define _PACKAGE_META_H_

class packageversion;

class packagemeta
{
public:
  packagemeta (char const *pkgname):installed_from (0), versions (0),
    versioncount (0), versionspace (0), installed (0), prev (0), exp (0)
  {
    name = new char[strlen (pkgname) + 1];
      strcpy (name, pkgname);
  };

  packagemeta (char const *pkgname,
	       char const *installedfrom):installed_from (0), versions (0),
    versioncount (0), versionspace (0), installed (0), prev (0), exp (0)
  {
    name = new char[strlen (pkgname) + 1];
    strcpy (name, pkgname);
    installed_from = new char[strlen (installedfrom) + 1];
    strcpy (installed_from, installedfrom);
  };


  ~packagemeta ()
  {
    delete name;
    if (installed_from)
      delete installed_from;
  };

  void add_version (packageversion &);
  void set_installed (packageversion &);
  void uninstall ();

  char *name;
  /* legacy variable used to output data for installed.db versions <= 2 */
  char *installed_from;

  /* this array is //NOT// sorted - too many pointer to get out of joint. */
  /* we can have member functions to return sorted details if desired */
  packageversion **versions;
  size_t versioncount;
  size_t versionspace;
  /* which one is installed. */
  packageversion *installed;
  /* which one is listed as "prev" in our available packages db */
  packageversion *prev;
  /* ditto for current - stable */
  packageversion *curr;
  /* and finally the experimental version */
  packageversion *exp;
};

#endif /* _PACKAGE_META_H_ */
