/*
 * Copyright (c) 2003 Igor Pechtchanski.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Igor Pechtchanski <pechtcha@cs.nyu.edu>
 *
 */

#ifndef SETUP_FILTERVISITOR_H
#define SETUP_FILTERVISITOR_H

#include "FindVisitor.h"
#include "String++.h"

/* For the wfd definition. See the TODO in find.cc */
#include "win32.h"

class Filter
{
public:
  virtual bool matchFile(String const &basePath, WIN32_FIND_DATA const *);
  virtual bool matchDirectory(String const &basePath, WIN32_FIND_DATA const *);
  virtual ~ Filter();
protected:
  Filter();
  Filter(Filter const &);
  Filter & operator= (Filter const &);
};

class FilterVisitor : public FindVisitor
{
public:
  virtual void visitFile(String const &basePath, WIN32_FIND_DATA const *);
  virtual void visitDirectory(String const &basePath, WIN32_FIND_DATA const *);
  FilterVisitor (FindVisitor *visitor, Filter *filter);
  virtual ~ FilterVisitor ();
protected:
  FilterVisitor ();
  FilterVisitor (FilterVisitor const &);
  FilterVisitor & operator= (FilterVisitor const &);
private:
  FindVisitor *_visitor;
  Filter *_filter;
};

class ExcludeNameFilter : public Filter
{
public:
  ExcludeNameFilter (String const &filePattern, String const &dirPattern = "");
  virtual ~ ExcludeNameFilter ();

  virtual bool matchFile(String const &basePath, WIN32_FIND_DATA const *);
  virtual bool matchDirectory(String const &basePath, WIN32_FIND_DATA const *);
protected:
  ExcludeNameFilter ();
  ExcludeNameFilter (ExcludeNameFilter const &);
  ExcludeNameFilter & operator= (ExcludeNameFilter const &);
private:
  String _filePattern;
  String _dirPattern;
};

#endif /* SETUP_FILTERVISITOR_H */
