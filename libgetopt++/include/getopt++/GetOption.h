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

#ifndef _GETOPTION_H_
#define _GETOPTION_H_

#include <getopt.h>
class Option;

class GetOption
{
public:
  static GetOption & GetInstance ();
  void Register (Option *);
  bool Process (int argc, char **argv);
private:
  static GetOption Instance;
  void Init ();
  int inited;			//we can't use a bool in case it is
  // non zero on startup.
  Option **options;
  int optCount;
};

#endif // _GETOPTION_H_
