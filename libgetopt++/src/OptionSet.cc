/*
 * Copyright (c) 2002 Robert Collins.
 * Copyright (c) 2003 Robert Collins.
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
#include "getopt++/DefaultFormatter.h"

#include <iostream>

using namespace std;

OptionSet::OptionSet () {}
OptionSet::~OptionSet ()
{
}

void
OptionSet::Init()
{
  options = std::vector<Option *> ();
}

bool
OptionSet::Process (int argc, char **argv, OptionSet *defaultOptionSet)
{
  if (argc == 1)
    {
//      log (LOG_TIMESTAMP, "No command line options pass\n");
      return true;
    }
  if (options.size() == 0)
    {
      if (defaultOptionSet)
	  return defaultOptionSet->Process (argc, argv);
//      log (LOG_TIMESTAMP,
//	   "%d Command line options passed, and no options registered\n",
//	   argc);
      return false;
    }
//  log (LOG_TIMESTAMP, "Process command line options\n");
  struct option longopts[options.size() + 1];
  string shortopts;
  for (std::vector<Option *>::iterator i = options.begin(); i != options.end(); ++i)
    {
      shortopts += (*i)->shortOption ();
      longopts[distance(options.begin(), i)] = (*i)->longOption ();
    }
  char const * opts = shortopts.c_str ();
  {
    struct option foo = {0, 0, 0, 0};
    longopts[options.size()] = foo;
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
	  for (std::vector<Option *>::iterator i = options.begin(); i != options.end(); ++i)
	    {
	      if (longopts[distance(options.begin(), i)].val == lastoption && !longopts[distance(options.begin(), i)].flag)
		(*i)->Process (optarg);
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
    options.push_back(anOption);
}

void
OptionSet::ParameterUsage (ostream &aStream)
{
  for_each (options.begin(), options.end(), DefaultFormatter (aStream));
}

std::vector<Option *> const &
OptionSet::optionsInSet() const
{
    return options;
}
