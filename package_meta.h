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

class genericpackage;

class packagemeta
{
public:
  packagemeta (char const *pkgname):versions (0), versioncount (0),
    versionspace (0), installed (0), prev (0), exp (0)
  {
    name = new char[strlen (pkgname) + 1];
      strcpy (name, pkgname);
  };
  ~packagemeta ()
  {
    delete name;
  };

  void add_version (genericpackage &);
  void set_installed (genericpackage &);


  /* array of versions of this package that we know about */
  char *name;
  /* this array is //NOT// sorted - too many pointer to get out of joint. */
  /* we can have member functions to return sorted details if desired */
  genericpackage **versions;
  size_t versioncount;
  size_t versionspace;
  /* which one is installed. */
  genericpackage *installed;
  /* which one is listed as "prev" in our available packages db */
  genericpackage *prev;
  /* ditto for current - stable */
  genericpackage *curr;
  /* and finally the experimental version */
  genericpackage *exp;
};

#endif /* _PACKAGE_META_H_ */
