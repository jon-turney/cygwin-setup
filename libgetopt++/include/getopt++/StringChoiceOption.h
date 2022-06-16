/*
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#ifndef _STRINGCHOICEOPTION_H_
#define _STRINGCHOICEOPTION_H_

#include <vector>
#include <getopt++/StringOption.h>

class StringChoiceOption : public StringOption
{
public:
  typedef std::vector<std::pair<const char *, int>> StringChoices;

  StringChoiceOption(StringChoices choices,
                     char shortopt, char const *longopt = 0,
                     unsigned int shorthelp = 0,
                     bool const optional = true,   // option without choice string is permitted
                     int const defaultvalue = -1,  // value when option is absent
                     int const impliedvalue = -1); // value when option is present without choice string

  virtual ~ StringChoiceOption ();
  virtual Result Process (char const *, int);
  operator int () const { return intval; }

private:
  StringChoices choices;
  int intval;
  int _impliedvalue;
};

#endif // _STRINGCHOICEOPTION_H_
