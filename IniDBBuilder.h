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

#ifndef SETUP_INIDBBUILDER_H
#define SETUP_INIDBBUILDER_H

#include "PackageSpecification.h"
#include "PackageTrust.h"

enum class hashType { none, md5, sha512 };

class IniDBBuilder
{
public:
  virtual ~IniDBBuilder() {};

  virtual void buildTimestamp (const std::string& ) = 0;
  virtual void buildVersion (const std::string& ) = 0;
  virtual const std::string buildMinimumVersion(const std::string &) = 0;
  virtual void buildPackage (const std::string& ) = 0;
  virtual void buildPackageVersion (const std::string& ) = 0;
  virtual void buildPackageSDesc (const std::string& ) = 0;
  virtual void buildPackageLDesc (const std::string& ) = 0;
  virtual void buildPackageInstall (const std::string&, const std::string&,
                                    char *, hashType) = 0;
  virtual void buildPackageSource (const std::string&, const std::string&,
                                   char *, hashType) = 0;
  virtual void buildPackageTrust (trusts) = 0;
  virtual void buildPackageCategory (const std::string& ) = 0;
  virtual void buildBeginDepends () = 0;
  virtual void buildBeginBuildDepends () = 0;
  virtual void buildBeginObsoletes () = 0;
  virtual void buildBeginProvides () = 0;
  virtual void buildBeginConflicts () = 0;
  virtual void buildMessage (const std::string&, const std::string&) = 0;
  virtual void buildSourceName (const std::string& ) = 0;
  virtual void buildSourceNameVersion (const std::string& ) = 0;
  virtual void buildPackageListNode (const std::string& ) = 0;
  virtual void buildPackageListOperator (PackageSpecification::_operators const &) = 0;
  virtual void buildPackageListOperatorVersion (const std::string& ) = 0;
  virtual void buildPackageReplaceVersionsList (const std::string& ) = 0;
  virtual void set_arch (const std::string& a) = 0;
  virtual void set_release (const std::string& rel) = 0;
};

#endif /* SETUP_INIDBBUILDER_H */
