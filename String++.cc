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
#include "concat.h"
#include "io_stream.h"

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

// able to cache the result if needed.
char *
String::cstr ()
{
  if (theData->length == 0)
    return NULL;
  char * tempcStr =new char[theData->length + 1];
  // remove when exceptions are done
  if (!tempcStr)
        exit (100);
  tempcStr[theData->length] = '\0';
  memcpy (tempcStr, theData->theString, theData->length);
  return tempcStr;
}

char *
String::cstr () const
{
  if (theData->length == 0)
    return NULL;
  char * tempcStr =new char[theData->length + 1];
  // remove when exceptions are done
  if (!tempcStr)
        exit (100);
  tempcStr[theData->length] = '\0';
  memcpy (tempcStr, theData->theString, theData->length);
  return tempcStr;
}

// able to cache the result if needed.
char const *
String::cstr_oneuse () const
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

// does this character exist in the string?
// 0 is false, 1 is the first position...
// XXX FIXME: Introduce npos, and change all
// if (size) calls to be if (size()==npos)
size_t
String::find(char aChar) const
{
  for (size_t i=0; i < theData->length; ++i)
    if (theData->theString[i] == aChar)
      return i+1;
  return 0;
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

String
String::concat (const char *aString, ...)
{
  va_list aList;
  va_start (aList, aString);

  return vconcat (aString, aList);  
}

String
String::vconcat (const char *aString, va_list aList)
{
  char *tempString = ::vconcat(aString, aList);
  String theString = String (tempString);
  delete[] tempString;
  return theString;
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

String
String::absorb (unsigned char *aString, size_t aLength)
{
  String theString;
  theString.theData->theString = aString;
  theString.theData->length = aLength;
  return theString;
}

int
String::casecompare (String const lhs, String const rhs)
{
  return lhs.casecompare (rhs);
}
