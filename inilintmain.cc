/*
 * Copyright (c) 2002, Robert Collins.
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

#include "io_stream.h"
#include "IniDBBuilderLint.h"
#include "cli/CliFeedback.h"
#include "ini.h"
#include <iostream>
#include <sstream>
#include "LogSingleton.h"

void
show_help()
{
  std::cout << "inilint checks cygwin setup.ini files and reports any errors" << std::endl;
}

int
main (int argc, char **argv)
{
  if (argc != 2)
    {
      show_help();
      return 1;
    }

  std::string inifilename = argv[1];

  // Note: this only accepts absolute pathnames
  io_stream *ini_file = io_stream::open ("file://" + inifilename, "rb", 0);
  if (!ini_file)
    {
      std::cerr << "could not open " << inifilename << std::endl;
      return 1;
    }

  CliFeedback feedback;
  IniDBBuilderLint builder;
  ini_init(ini_file, &builder, feedback);

  // Note: unrecognized lines are ignored by ignore_line(), so this is currently
  // only useful for finding where recognized lines don't fit the grammar.
  yyparse();

  return 0;
}

const std::string &
get_root_dir ()
{
  static std::string empty;
  return empty;
}

void
fatal (HWND owner, int id, ...)
{
  exit(1);
}
