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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "FilterVisitor.h"
#include "String++.h"
#include "find.h"

#include <iostream>

FilterVisitor::FilterVisitor(FindVisitor *visitor, Filter *filter)
  : _visitor(visitor), _filter(filter) {}

FilterVisitor::~FilterVisitor() {}

void
FilterVisitor::visitFile(String const &basePath, WIN32_FIND_DATA const *aFile)
{
  if (_filter->matchFile(basePath, aFile))
    _visitor->visitFile(basePath, aFile);
}

void
FilterVisitor::visitDirectory(String const &basePath, WIN32_FIND_DATA const *aDir)
{
  if (_filter->matchDirectory(basePath, aDir))
    _visitor->visitDirectory(basePath, aDir);
}

Filter::Filter() {}
Filter::~Filter() {}

bool
Filter::matchFile(String const &basePath, WIN32_FIND_DATA const *aFile)
{
  return true;
}

bool
Filter::matchDirectory(String const &basePath, WIN32_FIND_DATA const *aDir)
{
  return true;
}

ExcludeNameFilter::ExcludeNameFilter() : _filePattern(""), _dirPattern("") {}

ExcludeNameFilter::~ExcludeNameFilter(){}

ExcludeNameFilter::ExcludeNameFilter(String const &filePattern,
				     String const &dirPattern)
  : _filePattern(filePattern), _dirPattern(dirPattern) {}

bool
ExcludeNameFilter::matchFile(String const &basePath, WIN32_FIND_DATA const *aFile)
{
  return !(basePath + aFile->cFileName).matches(_filePattern);
}

bool
ExcludeNameFilter::matchDirectory(String const &basePath, WIN32_FIND_DATA const *aDir)
{
  return !(basePath + aDir->cFileName).matches(_dirPattern);
}

