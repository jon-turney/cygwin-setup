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

#ifndef _PACKAGE_DB_H_
#define _PACKAGE_DB_H_

/* required to parse this file */
#include "list.h"
//class CategoryList;
class Category;
class packagemeta;
class io_stream;

/*TODO: add mutexs */
class packagedb
{
public:
  packagedb ();
  packagemeta *getfirstpackage ();
  packagemeta *getnextpackage ();
  packagemeta *getpackagebyname (const char *);
  /* 0 on success */
  int addpackage (packagemeta &);
  /* 0 on success */
  int flush ();
  /* return an existing record if it exists, otherwise make a new one */
    packagemeta & registerpackage (char const *);
  size_t npackages ()
  {
    return packagecount;
  };
  /* all seen categories */
  static list < Category, char const *, strcasecmp > categories;
private:
  /* this gets sorted */
  static packagemeta **packages;
  static size_t packagecount;
  static size_t packagespace;
  size_t curr_package;
  io_stream *db;
  static int installeddbread;	/* do we have to reread this */
};

#endif /* _PACKAGE_DB_H_ */
