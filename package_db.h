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
#include <String++.h>
#include <vector>
#include <map>
#include "category.h"
class packagemeta;
class io_stream;
class PackageSpecification;

typedef enum {
  PackageDB_Install,
  PackageDB_Download
} PackageDBActions;

/*TODO: add mutexs */
class packagedb
{
public:
  packagedb ();
  /* 0 on success */
  int flush ();
  packagemeta * findBinary (PackageSpecification const &) const;
  packagemeta * findSource (PackageSpecification const &) const;
  /* all seen binary packages */
  static vector < packagemeta *> packages;
  /* all seen source packages */
  static vector <packagemeta *> sourcePackages;
  /* all seen categories */
  typedef map <String, vector <packagemeta *>, String::caseless > categoriesType;
  static categoriesType categories;
  static PackageDBActions task;
private:
  static int installeddbread;	/* do we have to reread this */
};

#endif /* _PACKAGE_DB_H_ */
