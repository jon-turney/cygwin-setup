/*
 * Copyright (c) 2002 Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <robertc@hotmail.com>
 *
 */

#include "GetOption.h"
#include "Option.h"
#include "log.h"

GetOption
  GetOption::Instance;

GetOption & GetOption::GetInstance ()
{
  if (Instance.inited != 42)
    {
      Instance.Init ();
    }
  return Instance;
}

bool GetOption::Process (int argc, char **argv)
{
  if (argc == 1)
    {
      log (LOG_TIMESTAMP, "No command line options pass\n");
      return true;
    }
  if (optCount == 0)
    {
      log (LOG_TIMESTAMP,
	   "%d Command line options passed, and no options registered\n",
	   argc);
      return false;
    }
  log (LOG_TIMESTAMP, "Process command line options\n");
  struct option
    longopts[optCount + 1];
  String
    shortopts;
  for (int i = 0; i < optCount; ++i)
    {
      Option *
	anOption =
	options[i];
      shortopts += anOption->shortOption ();
      longopts[i] = anOption->longOption ();
    }
  char *
    opts =
    shortopts.
    cstr ();
  {
    struct option
      foo = {
	0,
	0,
	0,
    0 };
    longopts[optCount] = foo;
  }
// where is this correctly defined?  opterr=0;
  int
    lastoption;
  while ((lastoption = getopt_long (argc, argv, opts, longopts, 0)) != -1)
    {
      if (lastoption)
	{
	  if (lastoption == '\?')
	    {
	      //ambigous option  
	      delete[]opts;
	      return false;
	    }
	  for (int i = 0; i < optCount; ++i)
	    {
	      if (longopts[i].val == lastoption && !longopts[i].flag)
		options[i]->Process (optarg);
	    }
	}
    }
  delete[]opts;
  return true;
}

void
GetOption::Init ()
{
  optCount = 0;
  options = 0;
  inited = 42;
}

//FIXME: check for conflicts.
void
GetOption::Register (Option * anOption)
{
  Option **t = new Option *[optCount + 1];
  for (int i = 0; i < optCount; ++i)
    t[i] = options[i];
  t[optCount++] = anOption;
  delete[]options;
  options = t;
}
