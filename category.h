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

class CategoryPackage;

class Category
{
public:
  Category ();
  Category (const char *);

  Category *next;		/* the next category in the list */
  const char *name;		/* the category */
  CategoryPackage *packages;	/* the packages in this category */
};


#endif /* _CATEGORY_H_ */
