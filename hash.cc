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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include <stdlib.h>
#include <string.h>

#include "hash.h"

class hash_internals
{
public:
  char **keys;
  int numkeys;
  int maxkeys;
  int prev_index;
};

hash::hash ()
{
  h = new hash_internals;
  h->numkeys = 0;
  h->maxkeys = 10;
  h->keys = (char **) malloc (h->maxkeys * sizeof (char *));
  h->prev_index = 0;
}

hash::~hash ()
{
  free (h->keys);
  free (h);
}


void
hash::add (char const *string)
{
  int i;
  for (i = 0; i < h->numkeys; i++)
    if (strcmp (h->keys[i], string) == 0)
      return;
  if (h->numkeys >= h->maxkeys)
    {
      h->maxkeys += 10;
      h->keys = (char **) realloc (h->keys, h->maxkeys * sizeof (char *));
    }

  h->keys[h->numkeys] = _strdup (string);
  h->numkeys++;
}


int
hash::has (char const *string)
{
  int i;
  for (i = 0; i < h->numkeys; i++)
    if (strcmp (h->keys[i], string) == 0)
      return 1;
  return 0;
}

char *
hash::enumerate (char const *prev)
{
  if (prev == 0)
    h->prev_index = -1;
  h->prev_index++;
  if (h->prev_index >= h->numkeys)
    return 0;
  return h->keys[h->prev_index];
}

static int
rev_len (const void *va, const void *vb)
{
  char *a = *(char **) va;
  char *b = *(char **) vb;
  return strcmp (b, a);
}

void
hash::reverse_sort ()
{
  qsort (h->keys, h->numkeys, sizeof (h->keys[0]), rev_len);
}
