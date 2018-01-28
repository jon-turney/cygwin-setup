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

#include "PackageSpecification.h"
#include <iostream>
#include "package_version.h"

PackageSpecification::PackageSpecification (const std::string& packageName)
  : _packageName (packageName) , _operator (Equals), _version ()
{
}

PackageSpecification::PackageSpecification (const std::string& packageName,
                                            const std::string& packageVersion)
  : _packageName (packageName) , _operator (Equals), _version (packageVersion)
{
}

const std::string&
PackageSpecification::packageName () const
{
  return _packageName;
}

const PackageSpecification::_operators
PackageSpecification::op() const
{
  return _operator;
}

const std::string&
PackageSpecification::version() const
{
  return _version;
}

void
PackageSpecification::setOperator (_operators anOperator)
{
  _operator = anOperator;
}

void
PackageSpecification::setVersion (const std::string& aVersion)
{
  _version = aVersion;
}

bool
PackageSpecification::satisfies (packageversion const &aPackage) const
{
  if (casecompare(_packageName, aPackage.Name()) != 0)
    return false;

  // The default values of _operator = Equals and _version is an empty-string
  // match any version
  if (_version.size())
    {
      int comparison = casecompare(aPackage.Canonical_version (), _version);
      switch (_operator)
        {
        case Equals:
          return (comparison == 0);
        case LessThan:
          return (comparison < 0);
        case MoreThan:
          return (comparison > 0);
        case LessThanEquals:
          return (comparison <= 0);
        case MoreThanEquals:
          return (comparison >= 0);
        default:
          return false;
        }
    }
  return true;
}

std::ostream &
operator << (std::ostream &os, PackageSpecification const &spec)
{
  os << spec._packageName;
  if (spec._operator)
    os << " " << PackageSpecification::caption(spec._operator) << " " << spec._version;
  return os;
}

char const *
PackageSpecification::caption (_operators _value)
{
  switch (_value)
    {
    case Equals:
    return "==";
    case LessThan:
    return "<";
    case MoreThan:
    return ">";
    case LessThanEquals:
    return "<=";
    case MoreThanEquals:
    return ">=";
    }
  // Pacify GCC: (all case options are checked above)
  return "Unknown operator";
}
