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
#include "PackageSpecification.h"

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
  virtual void buildPackageInstall (String const &);
  virtual void buildPackageSource (String const &, String const &);
  virtual void buildPackageTrust (int);
  virtual void buildPackageCategory (String const &);
  virtual void buildBeginDepends ();
  virtual void buildBeginPreDepends ();
  virtual void buildPriority (String const &);
  virtual void buildInstalledSize (String const &);
  virtual void buildMaintainer (String const &);
  virtual void buildArchitecture (String const &);
  virtual void buildInstallSize (String const &);
  virtual void buildInstallMD5 (unsigned char const[16]);
  virtual void buildSourceMD5 (unsigned char const[16]);
  virtual void buildBeginRecommends ();
  virtual void buildBeginSuggests ();
  virtual void buildBeginReplaces ();
  virtual void buildBeginConflicts ();
  virtual void buildBeginProvides ();
  virtual void buildBeginBuildDepends ();
  virtual void buildBeginBinary ();
  virtual void buildDescription (String const &);
  virtual void buildSourceName (String const &);
  virtual void buildSourceNameVersion (String const &);
  virtual void buildPackageListAndNode ();
  virtual void buildPackageListOrNode (String const &);
  virtual void buildPackageListOperator (PackageSpecification::_operators const &);
  virtual void buildPackageListOperatorVersion (String const &);

  unsigned int timestamp;
  String version;
  String parse_mirror;
};

#endif /* _INIDBBUILDER_H_ */
