/*
 * Copyright (c) 2001, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

/* this is the parent class for all package source (not source code - installation
 * source as in http/ftp/disk file) operations.
 */

#ifndef _EXCEPTION_H_
#define _EXCEPTION_H_

#include <exception>
#include "String++.h"

/* Generic excpetion class for throwing exceptions */
class Exception : public std::exception {
public:
  Exception (char const *where, char const *message, int appErrNo = 0);
  Exception (char const *where, const String &message, int appErrNo = 0);
  ~Exception () throw () {}
  char const *what() const throw();
  int errNo() const;
private:
  String _message;
  int appErrNo;
};

// Where should these live?
#define APPERR_CORRUPT_PACKAGE	1
#define APPERR_IO_ERROR		2
#define APPERR_LOGIC_ERROR	3

#endif /* _EXCEPTION_H_ */
