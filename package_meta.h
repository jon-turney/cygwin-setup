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
class packagemeta;

/* Required to parse this completely */
#include "category_list.h"
#include "list.h"
#include "strings.h"

/* 
   For cleanliness this may need to be put in its own file later. */
class CategoryPackage
{
public:
  CategoryPackage (packagemeta & package):next (0), pkg (package)
  {
  };
  CategoryPackage *next;	/* The next package pointer in the list */
  packagemeta & pkg;
};


/* NOTE: A packagemeta without 1 packageversion is invalid! */
class packagemeta
{
public:
  packagemeta (char const *pkgname):installed_from (0),
    //versions (0),
//    versioncount (0), versionspace (0), 
  installed (0), prev (0), prevtimestamp (0), curr (0), currtimestamp (0),
    exp (0), exptimestamp (0), desired (0)
  {
    name = new char[strlen (pkgname) + 1];
      strcpy (name, pkgname);
  };

  packagemeta (char const *pkgname,
	       char const *installedfrom):installed_from (0),
    //versions (0),    versioncount (0), versionspace (0), 
   
    installed (0), prev (0), prevtimestamp (0), curr (0), currtimestamp (0),
    exp (0), exptimestamp (0), desired (0)
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

  char *name;			/* package name, like "cygwin" */
  /* legacy variable used to output data for installed.db versions <= 2 */
  char *installed_from;
  /* SDesc is global in theory, across all package versions. 
     LDesc is not: it can be different per version */
  char const *SDesc ();
  /* what categories does this package belong in. Note that if multiple versions
   * of a package disagree.... the first one read in will take precedence.
   */
  CategoryList & Categories ()
  {
    return categories;
  };
  /* Add a known category to this objects category list.
   */
  void add_category (Category &);

  list < packageversion, char const *, strcasecmp > versions;
  /* which one is installed. */
  packageversion *installed;
  /* which one is listed as "prev" in our available packages db */
  packageversion *prev;
  /* And what was the timestamp of the ini it was found from */
  unsigned int prevtimestamp;
  /* ditto for current - stable */
  packageversion *curr;
  unsigned int currtimestamp;
  /* and finally the experimental version */
  packageversion *exp;
  unsigned int exptimestamp;
  /* Now for the user stuff :] */
  /* What version does the user want ? */
  packageversion *desired;
private:
  CategoryList categories;
};

#endif /* _PACKAGE_META_H_ */
