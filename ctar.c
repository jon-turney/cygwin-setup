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

#include <stdio.h>
#include "tar.h"

extern int _tar_verbose;

int
main (int argc, char **argv)
{
  char **map = (char **) malloc ((argc+2) * sizeof (char *));
  int i, j;

  while (argc > 1 && strcmp (argv[1], "-v") == 0)
    {
      _tar_verbose ++;
      argc--;
      argv++;
    }

  if (argc < 2)
    {
      fprintf (stderr, "Usage: ctar <file.tar.gz> [from to [from to ...]]\n");
      exit (1);
    }

  j = 0;
  for (i = 2; i < argc; i ++)
    map[j++] = argv[i];
  map[j++] = 0;
  map[j++] = 0;

  i = tar_auto (argv[1], map);

  if (i == 1)
    fprintf (stderr, "ctar: there was an error extracting %s\n", argv[1]);
  else if (i > 1)
    fprintf (stderr, "ctar: there were errors extracting %s\n", argv[1]);

  return i;
}
