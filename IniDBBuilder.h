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

#ifndef _INIDBBUILDER_H_
#define _INIDBBUILDER_H_

#include "String++.h"

class IniDBBuilder
{
public:
  virtual ~IniDBBuilder();
  virtual void buildTimestamp (String const &);
  virtual void buildVersion (String const &);
  virtual void buildPackage (String const &);
  virtual void buildPackageVersion (String const &);
  virtual void buildPackageSDesc (String const &);
  virtual void buildPackageLDesc (String const &);
  virtual void buildPackageInstall (String const &, String const &,
				    String const & = String());
  virtual void buildPackageSource (String const &, String const &,
				   String const & = String());
  virtual void buildPackageTrust (int);
  virtual void buildPackageRequirement (String const &);
  virtual void buildPackageCategory (String const &);

  unsigned int timestamp;
  String version;
  String parse_mirror;
};

#endif /* _INIDBBUILDER_H_ */
