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
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

#if 0
static const char *cvsid = "\n%%% $Id$\n";
#endif

#include "PackageSpecification.h"
#include "package_version.h"

PackageSpecification::PackageSpecification (String const &packageName)
  : _packageName (packageName) , _operator (0), _version ()
{
}

String const&
PackageSpecification::packageName () const
{
  return _packageName;
}

void
PackageSpecification::setOperator (_operators const &anOperator)
{
  _operator = &anOperator;
}

void
PackageSpecification::setVersion (String const &aVersion)
{
  _version = aVersion;
}

bool
PackageSpecification::satisfies (packageversion const &aPackage) const
{
  return false;
}

String
PackageSpecification::serialise () const
{
  return _packageName;
}

PackageSpecification &
PackageSpecification::operator= (PackageSpecification const &rhs)
{
  _packageName = rhs._packageName;
  return *this;
}

std::ostream &
operator << (std::ostream &os, PackageSpecification const &spec)
{
  os << spec._packageName;
  if (spec._operator)
    os << " " << spec._operator->caption() << " " << spec._version;
  return os;
}

const PackageSpecification::_operators PackageSpecification::Equals(0);
const PackageSpecification::_operators PackageSpecification::LessThan(1);
const PackageSpecification::_operators PackageSpecification::MoreThan(2);
const PackageSpecification::_operators PackageSpecification::LessThanEquals(3);
const PackageSpecification::_operators PackageSpecification::MoreThanEquals(4);

char const *
PackageSpecification::_operators::caption () const
{
  switch (_value)
    {
    case 0:
    return "==";
    case 1:
    return "<";
    case 2:
    return ">";
    case 3:
    return "<=";
    case 4:
    return ">=";
    }
  // Pacify GCC: (all case options are checked above)
  return "Unknown operator";
}
