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

#include <stdlib.h>
#include <string.h>

#include "category_list.h"
#include "category.h"

CategoryList::CategoryList ():_categories (0), ncategories (0),
categoriesspace (0)
{
}

Category *
CategoryList::register_category (const char *catname)
{
  Category *tempcat = getcategorybyname (catname);
  if (!tempcat)
    {
      if (ncategories == categoriesspace)
	{
	  Category **newcategories = (Category **) realloc (_categories,
							    sizeof (Category
								    *) *
							    (categoriesspace +
							     20));
	  if (!newcategories)
	    {
	      //die
	      exit (100);
	    }
	  _categories = newcategories;
	  if (categoriesspace == 0)
	    _categories[0] = 0;
	  categoriesspace += 20;
	}
      tempcat = new Category (catname);
      size_t n;
      for (n = 0;
	   n < ncategories
	   && strcasecmp (_categories[n]->name, tempcat->name) < 0; n++);
      /* insert at n */
      if (n)
	_categories[n - 1]->next = tempcat;
      tempcat->next = ncategories >= n ? _categories[n] : 0;
      memmove (&_categories[n + 1], &_categories[n],
	       ncategories * sizeof (Category *));
      _categories[n] = tempcat;
      ncategories++;
    }
  return tempcat;
}

Category *
CategoryList::getcategorybyname (const char *name)
{
  for (size_t n = 0; n < ncategories; n++)
    if (strcasecmp (_categories[n]->name, name) == 0)
      return _categories[n];
  return 0;
}
