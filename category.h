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

/* categories for packages */

#ifndef _CATEGORY_H_
#define _CATEGORY_H_

#include "String++.h"
#include <vector>

class Category;
class packagemeta;

int Categorycmp (Category const &, Category const &);
// Grrr template problem - couldn't get list to use int Ucmp (U const, U const),
// and gcc wouldn't cast on the fly.
int Categorycmp (Category &, Category &);

class Category
{
public:
  Category ();
  Category (String const &);
  bool operator== (Category const &rhs) const {return Categorycmp (*this, rhs) ? false : true;}
  bool operator < (Category const &rhs) const {return Categorycmp (*this, rhs) < 0;}

  Category *next;		/* the next category in the list */
  String const name;			/* the category */
  String const key;		/* always == name */
  vector <packagemeta *> packages; /* the packages in this category */
};

#endif /* _CATEGORY_H_ */
