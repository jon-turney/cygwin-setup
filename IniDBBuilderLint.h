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

#ifndef SETUP_INIDBBUILDERLINT_H
#define SETUP_INIDBBUILDERLINT_H

#include "IniDBBuilder.h"

class IniDBBuilderLint:public IniDBBuilder
{
public:
  virtual ~IniDBBuilderLint() {};

  virtual void buildTimestamp (const std::string& ) {};
  virtual void buildVersion (const std::string& ) {};
  virtual const std::string buildMinimumVersion(const std::string &s) { return ""; }
  virtual void buildPackage (const std::string& ) {};
  virtual void buildPackageVersion (const std::string& ) {};
  virtual void buildPackageSDesc (const std::string& ) {};
  virtual void buildPackageLDesc (const std::string& ) {};
  virtual void buildPackageInstall (const std::string&, const std::string&,
                                    char *, hashType) {};
  virtual void buildPackageSource (const std::string&, const std::string&,
                                   char *, hashType) {};
  virtual void buildPackageTrust (trusts) {};
  virtual void buildPackageCategory (const std::string& ) {};
  virtual void buildBeginDepends () {};
  virtual void buildBeginBuildDepends () {};
  virtual void buildBeginObsoletes () {};
  virtual void buildBeginProvides () {};
  virtual void buildBeginConflicts () {};
  virtual void buildMessage (const std::string&, const std::string&) {};
  virtual void buildSourceName (const std::string& ) {};
  virtual void buildSourceNameVersion (const std::string& ) {};
  virtual void buildPackageListNode (const std::string& ) {};
  virtual void buildPackageListOperator (PackageSpecification::_operators const &) {};
  virtual void buildPackageListOperatorVersion (const std::string& ) {};
  virtual void buildPackageReplaceVersionsList (const std::string& ) {};
  virtual void set_arch (const std::string& a) {};
  virtual void set_release (const std::string& rel) {};
};

#endif /* SETUP_INIDBBUILDERLINT_H */
