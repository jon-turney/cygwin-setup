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

#ifndef CINSTALL_STRING_H
#define CINSTALL_STRING_H

#include <stdarg.h>
#include <sys/types.h>
#include <iosfwd>

class io_stream;
class String {
  class _data;
public:
  // Static members first
  inline String();
  inline String (String const &);
  // We're notperformance bottlenecked.
  String (const char *); 
  inline String & operator = (String const &);
  ~String();
  // Up to the user to delete[] these.
  char * cstr();
  char * cstr() const; // may be less optimal
  char const * cstr_oneuse() const; // only valid until the next mutator call
  			      // pretends to be const !!
  inline size_t size() const; // number of characters (!= storage size).
  size_t find (char) const;
  // operator == and != can be done if/when we have a 'casesensitive' flag to
  // the constructors
  // - means this sorts to the left of the parameter
  int compare (String const &, size_t const = 0) const;
  static int compare (String const &, String const &, size_t const = 0);
  int casecompare (String const &, size_t const = 0) const;
  static int casecompare (String const &, String const &, size_t const = 0);
  static int casecompare (String const, String const);
  String &append (String const &);
  String &operator += (String const &);
  String operator + (String const &) const;
  String operator + (char const *) const;
  bool operator == (String const &) const;
  bool operator == (char const *) const;
    
private:
  class _data {
  public:
    _data ();
    _data (size_t);
    _data (_data const &);
    ~_data ();
    unsigned count; //Invariant: all constructors set to 1;
    // For now, char *, but can be TCHAR, or even UNICODE
    // when time permits.
    unsigned char *theString;
    char *cstr; // cached/oneuse Cstr encoded version
    size_t length;
  } *theData; // Invariant, there is always an 
  static String absorb (unsigned char *, size_t);
};

ostream &
operator << (ostream &os, String const &theString);

String::String() : theData (new _data) {}
String::String(String const &aString) : theData (aString.theData) 
{
  ++theData->count;
}

String &
String::operator= (String const &aString)
{
  // Don't touch the order
  ++aString.theData->count;
  if (--theData->count == 0) 
    delete theData;
  theData = aString.theData;
  return *this;
}

size_t 
String::size() const
{
  return theData->length;
}

#endif // CINSTALL_STRING_H
