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

/* a list categories for packages */

#ifndef _CATEGORY_LIST_H_
#define _CATEGORY_LIST_H_

class Category;

class CategoryList
{
public:
  Category & register_category (const char *name);
  Category *getcategorybyname (const char *name);
  int categories ()
  {
    return ncategories;
  };
  Category *getfirstcategory ()
  {
    return *_categories;
  };
  CategoryList ();
private:
  Category ** _categories;
  size_t ncategories;
  size_t categoriesspace;
};


#endif /* _CATEGORY_LIST_H_ */
