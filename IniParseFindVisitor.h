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

#ifndef SETUP_INIPARSEFINDVISITOR_H
#define SETUP_INIPARSEFINDVISITOR_H

#include "FindVisitor.h"
#include "String++.h"

/* parse passed in setup.ini files from disk. */
class IniDBBuilder;
class IniParseFeedback;
/* IniParse files and create a package db when no cached .ini exists */
class IniParseFindVisitor : public FindVisitor
{
public:
  IniParseFindVisitor (IniDBBuilder &aBuilder, String const &localroot, IniParseFeedback &);
  virtual void visitFile(String const &basePath, const WIN32_FIND_DATA *);
  virtual ~ IniParseFindVisitor ();

  unsigned int timeStamp() const;
  String version() const;
  int iniCount() const;
protected:
  IniParseFindVisitor (IniParseFindVisitor const &);
  IniParseFindVisitor & operator= (IniParseFindVisitor const &);
private:
  IniDBBuilder &_Builder;
  IniParseFeedback &_feedback;
  unsigned int baseLength;
  int local_ini;
  char *error_buf;
  int error_count;
  unsigned int setup_timestamp;
  String setup_version;
};

#endif /* SETUP_INIPARSEFINDVISITOR_H */
