/*
 * Copyright (c) 2000, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

/* Simple hash class for install.cc */

#include  "String++.h"

class hash_internals;

class hash
{
  hash_internals *h;
public:
    hash ();
   ~hash ();

  void add (char const *string);
  int has (char const *string);

  /* FIXME: this specialty should be done via a derived class */
  /* specialty for install.cc */
  void add_subdirs (String const &path);
  void reverse_sort ();

  char *enumerate (char const *prev = 0);
};
