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

#if HAVE_CONFIG_H
#include "autoconf.h"
#endif
#include "getopt++/OptionSet.h"
#include "getopt++/Option.h"

#include <iostream>

OptionSet::OptionSet () : options(0), optCount (0) {}
OptionSet::~OptionSet ()
{
  delete[] options;
}

void
OptionSet::Init()
{
  options = 0;
  optCount = 0;
}

bool OptionSet::Process (int argc, char **argv, OptionSet *defaultOptionSet)
{
  if (argc == 1)
    {
//      log (LOG_TIMESTAMP, "No command line options pass\n");
      return true;
    }
  if (optCount == 0)
    {
//      log (LOG_TIMESTAMP,
//	   "%d Command line options passed, and no options registered\n",
//	   argc);
      return false;
    }
//  log (LOG_TIMESTAMP, "Process command line options\n");
  struct option longopts[optCount + 1];
  String
    shortopts;
  for (int i = 0; i < optCount; ++i)
    {
      Option *anOption = options[i];
      shortopts += anOption->shortOption ();
      longopts[i] = anOption->longOption ();
    }
  char const *
    opts = shortopts.c_str ();
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
  int lastoption;
  while ((lastoption = getopt_long (argc, argv, opts, longopts, 0)) != -1)
    {
      if (lastoption)
	{
	  if (lastoption == '\?')
	    {
	      //ambigous option  
#if HAVE_STRING___H
	      delete[]opts;
#endif
	      return false;
	    }
	  for (int i = 0; i < optCount; ++i)
	    {
	      if (longopts[i].val == lastoption && !longopts[i].flag)
		options[i]->Process (optarg);
	    }
	}
    }
    if (optind < argc && optind > 0 && defaultOptionSet)
      return defaultOptionSet->Process (argc - optind, &argv[optind]);
#if HAVE_STRING___H
  delete[]opts;
#endif
  return true;
}

//FIXME: check for conflicts.
void
OptionSet::Register (Option * anOption)
{
  Option **t = new Option *[optCount + 1];
  for (int i = 0; i < optCount; ++i)
    t[i] = options[i];
  t[optCount++] = anOption;
  delete[]options;
  options = t;
}

/* Show the options on the left, the short description on the right.
 * descriptions must be < 40 characters in length
 */
void
OptionSet::ParameterUsage (ostream &aStream)
{
  for (int i = 0; i < optCount; ++i)
    {
      Option *anOption = options[i];
      String output = String() + " -" + anOption->shortOption ()[0];
      output += " --" ;
      output += anOption->longOption ().name;
      output += String (40 - output.size(), ' ');
      output += anOption->shortHelp();
      aStream << output << endl;
    }
}
