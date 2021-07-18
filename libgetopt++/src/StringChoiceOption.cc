/*
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#include <getopt++/StringChoiceOption.h>

StringChoiceOption::StringChoiceOption (StringChoices _choices,
                                        char shortopt,
                                        char const *longopt,
                                        std::string const &shorthelp,
                                        bool const optional,
                                        int const defaultvalue,
                                        int const impliedvalue) :
  StringOption ("", shortopt, longopt, shorthelp, optional),
  choices(_choices), intval (defaultvalue), _impliedvalue(impliedvalue)
{
};

StringChoiceOption::~ StringChoiceOption () {};

Option::Result
StringChoiceOption::Process (char const *optarg, int prefixIndex)
{
  Result res = StringOption::Process (optarg, prefixIndex);
  if (res != Ok)
    return res;

  const std::string& strval = *this;
  if (strval.empty())
    {
      intval = _impliedvalue;
      return Ok;
    }

  for (StringChoiceOption::StringChoices::const_iterator i = choices.begin();
       i != choices.end();
       i++)
    {
      if (strval == i->first)
        {
          intval = i->second;
          return Ok;
        }
    }

  return Failed;
}
