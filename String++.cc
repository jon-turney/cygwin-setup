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
 * Written by Robert Collins.
 *
 */

// A String class to replace all the char * manipulation. 

#include "String++.h"
#include <string.h>
#include <ctype.h>
#include "io_stream.h"
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

using namespace std;

// _data

String::_data::_data(_data const &aData) : count (1), theString (new unsigned char[aData.length]), cstr(0), length (aData.length) {
  memcpy (theString, aData.theString, aData.length);
}

String::_data::_data(): count (1), theString(new unsigned char[0]), cstr (0), length (0) {}
String::_data::_data(size_t aLength): count (1), theString(new unsigned char[aLength]), cstr(0), length (aLength) {}
String::_data::~_data ()
{
    if (theString)
          delete[] theString;
      if (cstr)
	    delete[] cstr;
}

//String

String::String (const char *acString) : theData (new _data(acString ? strlen(acString) : 0))
{
  memcpy (theData->theString, acString, theData->length);
}

String::~String ()
{
  if (--theData->count == 0)
    delete theData;
}

String::String (string const &aString) : theData (new _data (aString.c_str() ? strlen (aString.c_str()) : 0))
{
  memcpy (theData->theString, aString.c_str(), theData->length);
}

// able to cache the result if needed.
char const *
String::c_str () const
{
  if (theData->length == 0)
    return NULL;
  if (theData->cstr)
    delete[] theData->cstr;
  theData->cstr = new char[theData->length + 1];
  theData->cstr[theData->length] = '\0';
  memcpy (theData->cstr, theData->theString, theData->length);
  return theData->cstr;
}

String
String::substr(size_t start, int len) const
{
  // Adapt the C++ string class
  return string(c_str()).substr(start, len);
}

int
String::compare (String const &aString, size_t const count) const
{
  // trivial cases:
  if (theData == aString.theData)
    return 0;
  size_t length = count ? count : theData->length;
  if (length > theData->length)
    length = theData->length;
  if (length > aString.theData->length)
    length = aString.theData->length;
  size_t i;
  for (i=0; i < length ; ++i)
    if (theData->theString[i] < aString.theData->theString[i])
      return -1;
    else if (theData->theString[i] > aString.theData->theString[i])
      return 1;
  // equal for length
  if (i == count && count != 0)
    return 0;
  if (theData->length < aString.theData->length)
    return -1;
  else if (theData->length > aString.theData->length)
    return 1;
  return 0;
}

int
String::casecompare (String const &aString, size_t const count) const
{
  // trivial cases:
  if (theData == aString.theData)
    return 0;
  size_t length = count ? count : theData->length;
  if (length > theData->length)
    length = theData->length;
  if (length > aString.theData->length)
    length = aString.theData->length;
  size_t i;
  for (i=0; i < length; ++i)
    if (toupper(theData->theString[i]) < toupper(aString.theData->theString[i]))
      return -1;
    else if (toupper(theData->theString[i]) > toupper(aString.theData->theString[i]))
      return 1;
  // equal for length
  if (i == count && count != 0)
    return 0;
  if (theData->length < aString.theData->length)
    return -1;
  else if (theData->length > aString.theData->length)
    return 1;
  return 0;
}

String &
String::operator+= (String const &aString)
{
  if (theData->count > 1) 
  {
    _data * someData = new _data(*theData);
    --theData->count;
    theData = someData;
  }

  unsigned char *tempString = theData->theString;
  theData->theString = new unsigned char [theData->length + aString.theData->length];
  // remove when exceptions are done
  if (!theData->theString)
    exit (100);
  memcpy (theData->theString, tempString, theData->length);
  delete[] tempString;
  memcpy (&theData->theString[theData->length], aString.theData->theString, aString.theData->length);
  theData->length += aString.theData->length;
  return *this;
}

String
String::operator + (String const &aString) const
{
  unsigned char *tempcString = new unsigned char [theData->length + aString.theData->length];
  // remove when exceptions are done
  if (!tempcString)
      exit (100);
  memcpy (tempcString, theData->theString, theData->length);
  memcpy (&tempcString[theData->length], aString.theData->theString, aString.theData->length);
  return absorb (tempcString, theData->length + aString.theData->length);
}

String
String::operator + (char const *aString) const
{
  // expensive, but quick to code.
  return *this + String (aString);
}

bool
String::operator == (String const &rhs) const
{
  return compare (rhs) ? false : true;
}

bool
String::operator == (char const *rhs) const
{
    return compare (rhs) ? false : true;
}

bool
String::operator != (String const &rhs) const
{
  return !(*this == rhs);
}

bool
String::operator != (char const *rhs) const
{
  return !(*this == rhs);
}

String
String::absorb (unsigned char *aString, size_t aLength)
{
  String theString;
  theString.theData->theString = aString;
  theString.theData->length = aLength;
  return theString;
}

char *
new_cstr_char_array (const String &s)
{
  size_t len = s.size();
  char *buf = new char[len + 1];
  if (len)
    memcpy (buf, s.c_str (), len);
  buf[len] = 0;
  return buf;
}

/* TODO: research how wide char and unicode interoperate with
 * C++ streams
 */
ostream &
operator << (ostream &os, String const &theString)
{
  os << theString.c_str();
  return os;
}

String
format_1000s(const int num, char sep)
{
  int mult = 1;
  while (mult * 1000 < num)
    mult *= 1000;
  ostringstream os;
  os << ((num / mult) % 1000);
  for (mult /= 1000; mult > 0; mult /= 1000)
    {
      int triplet = (num / mult) % 1000;
      os << sep;
      if (triplet < 100) os << '0';
      if (triplet < 10) os << '0';
      os << triplet;
    }
  return String(os.str());
}

std::string
stringify(int num)
{
  std::ostringstream os;
  os << num;
  return os.str();
}

int
casecompare (const std::string& a, const std::string& b, size_t limit)
{
  size_t length_to_check = std::min(a.length(), b.length());
  if (limit && length_to_check > limit)
    length_to_check = limit;

  size_t i;
  for (i = 0; i < length_to_check; ++i)
    if (toupper(a[i]) < toupper(b[i]))
      return -1;
    else if (toupper(a[i]) > toupper(b[i]))
      return 1;

  // Hit the comparison limit without finding a difference
  if (limit && i == limit) 
    return 0;

  if (a.length() < b.length())
    return -1;
  else if (a.length() > b.length())
    return 1;

  return 0;
}

std::string
replace(const std::string& haystack, const std::string& needle,
	const std::string& replacement)
{
  std::string rv(haystack);
  size_t n_len = needle.length(), r_len = replacement.length(),
	 search_start = 0;
  
  while (true)
  {
    size_t pos = rv.find(needle, search_start);
    if (pos == std::string::npos)
      return rv;
    rv.replace(pos, n_len, replacement);
    search_start = pos + r_len;
  }
}
