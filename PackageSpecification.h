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

#ifndef SETUP_PACKAGESPECIFICATION_H
#define SETUP_PACKAGESPECIFICATION_H

#include <iosfwd>
#include "String++.h"

class SolvableVersion;
typedef SolvableVersion packageversion;

/* Describe a package - i.e. we need version 5 of apt */

class PackageSpecification
{
public:
  PackageSpecification () : _packageName (), _operator(Equals) {}
  PackageSpecification (const std::string& packageName);
  PackageSpecification (const std::string& packageName,
                        const std::string &packageVersion);
  ~PackageSpecification () {}

  enum _operators
  {
    Equals,
    LessThan,
    MoreThan,
    LessThanEquals,
    MoreThanEquals,
  };

  const std::string& packageName() const;
  const _operators op() const;
  const std::string& version() const;

  void setOperator (_operators);
  void setVersion (const std::string& );

  bool satisfies (packageversion const &) const;

  friend std::ostream &operator << (std::ostream &, PackageSpecification const &);

private:
  static char const * caption (_operators _value);

  std::string _packageName; /* foobar */
  _operators  _operator;    /* >= */
  std::string _version;     /* 1.20 */
};

std::ostream &
operator << (std::ostream &os, PackageSpecification const &);

#endif /* SETUP_PACKAGESPECIFICATION_H */
