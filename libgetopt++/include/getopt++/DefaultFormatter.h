/*
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

#ifndef _GETOPT___DEFAULTFORMATTER_H_
#define _GETOPT___DEFAULTFORMATTER_H_

#include <iostream>
#include <vector>
#include "getopt++/Option.h"

/* Show the options on the left, the short description on the right.
 * descriptions must be < 40 characters in length
 */
class DefaultFormatter {
  public:
    DefaultFormatter (std::ostream &aStream) : theStream(aStream) {}
    void operator () (Option *anOption) {
      std::string output = std::string() + " -" + anOption->shortOption ()[0];
      output += " --" ;
      output += anOption->longOption ();
      output += std::string (40 - output.size(), ' ');
      std::string helpmsg = anOption->shortHelp();
      while (helpmsg.size() > 40)
	{
	  // TODO: consider using a line breaking strategy here.
	  int pos = helpmsg.substr(0,40).find_last_of(" ");
	  output += helpmsg.substr(0,pos);
	  helpmsg.erase (0,pos+1);
	  theStream << output << std::endl;
	  output = std::string (40, ' ');
	}
      output += helpmsg;
      theStream << output << std::endl;
    }
    std::ostream &theStream;
};


#endif // _GETOPT___DEFAULTFORMATTER_H_
