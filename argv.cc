/*
 * Copyright (c) 2001, Red Hat
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by R B Collins <rbtcollins@hotmail.com>
 *
 */

#include "win32.h"

#include <stdio.h>
#include <stdlib.h>

#include "argv.h"

static inline int
isquote (char c)
{
  char ch = c;
  return ch == '"' || ch == '\'';
}

/* Step over a run of characters delimited by quotes */
static /*__inline*/ char *
quoted (char *cmd)
{
  char quote = *cmd;

  char *p;
  strcpy (cmd, cmd + 1);
  if ((p = strchr (cmd, quote)) != NULL)
    strcpy (p, p + 1);
  else
    p = strchr (cmd, '\0');
  return p;
}

#undef issep
#define issep(ch) (strchr (" \t\n\r", (ch)) != NULL)

/* Build argv, argc from string passed from Windows.  */
static void
build_argv (char *cmd, char **&argv, int &argc) FAST_FUNCTION;

static void
build_argv (char *cmd, char **&argv, int &argc)
{
  int argvlen = 0;
  int nesting = 0;              // monitor "nesting" from insert_file

  argc = 0;
  argvlen = 0;
  argv = NULL;

  /* Scan command line until there is nothing left. */
  while (*cmd)
    {
      /* Ignore spaces */
      if (issep (*cmd))
        {
          cmd++;
          continue;
        }

      /* Found the beginning of an argument. */
      char *word = cmd;
      char *sawquote = NULL;
      while (*cmd)
        {
          if (*cmd != '"' && (*cmd != '\''))
            cmd++;              // Skip over this character
          else
            /* Skip over characters until the closing quote */
            {
              sawquote = cmd;
              cmd = quoted (cmd);
            }
          if (issep (*cmd))     // End of argument if space
            break;
        }
      if (*cmd)
        *cmd++ = '\0';          // Terminate `word'

      /* See if we need to allocate more space for argv */
      if (argc >= argvlen)
        {
          argvlen = argc + 10;
          argv = (char **) realloc (argv, (1 + argvlen) * sizeof (argv[0]));
        }

      /* Add word to argv file */
          argv[argc++] = word;
    }

  argv[argc] = NULL;
}


int
CommandLineToArgV(int &argc, char **&argv)
{

      char *line = GetCommandLineA ();
      line = strcpy ((char *) alloca (strlen (line) + 1), line);

      /* Scan the command line and build argv. */
      build_argv (line, argv, argc);
      if (GetCommandLineA () && argc < 2)
	return 1;
      return 0;
}
