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

#ifndef SETUP_STRING___H
#define SETUP_STRING___H

// A String class to replace all the char * manipulation. 

#include <stdarg.h>
#include <sys/types.h>
#include <iosfwd>
#include <string>

class io_stream;
class String {
  class _data;
public:
  // Static members first
  inline String();
  inline String (String const &);
  // We're notperformance bottlenecked.
  String (const char *); 
  String (int const);
  String (std::string const &);
  inline String & operator = (String const &);
  ~String();
  // Up to the user to delete[] these.
  char * cstr();
  char * cstr() const; // may be less optimal
  char const * cstr_oneuse() const; // only valid until the next mutator call
  			      // pretends to be const !!
  inline size_t size() const; // number of characters (!= storage size).
  size_t find (char) const;
  String substr (size_t start = 0, int len = -1) const;
  // operator == and != can be done if/when we have a 'casesensitive' flag to
  // the constructors
  // - means this sorts to the left of the parameter
  int compare (String const &, size_t const = 0) const;
  int casecompare (String const &, size_t const = 0) const;
  String &append (String const &);
  String &operator += (String const &);
  String operator + (String const &) const;
  String operator + (char const *) const;
  bool operator == (String const &) const;
  bool operator == (char const *) const;
  bool operator != (String const &) const;
  bool operator != (char const *) const;
  struct caseless { bool operator () (String const &s1, String const &s2) const
	{
	  return s1.casecompare (s2) < 0;
	}};
  String replace (char pattern, char replacement) const;
  String replace (String const &pattern, String const &replacement) const;

  operator std::string() const {
    return std::string( size() ? cstr_oneuse() : "" );
  };
    
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

std::ostream &
operator << (std::ostream &os, String const &theString);

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

#define __TOSTRING__(X) #X
/* Note the layer of indirection here is needed to allow
   stringification of the expansion of macros, i.e. "#define foo
   bar", "TOSTRING(foo)", to yield "bar". */
#define TOSTRING(X) __TOSTRING__(X)

#endif /* SETUP_STRING___H */
