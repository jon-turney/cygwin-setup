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

class IniDBBuilder
{
public:
  virtual ~IniDBBuilder();
  virtual void buildTimestamp (const std::string& );
  virtual void buildVersion (const std::string& );
  virtual void buildPackage (const std::string& );
  virtual void buildPackageVersion (const std::string& );
  virtual void buildPackageSDesc (const std::string& );
  virtual void buildPackageLDesc (const std::string& );
  virtual void buildPackageInstall (const std::string& );
  virtual void buildPackageSource (const std::string&, const std::string&);
  virtual void buildSourceFile (unsigned char const[16],
                                const std::string&, const std::string&);
  virtual void buildPackageTrust (int);
  virtual void buildPackageCategory (const std::string& );
  virtual void buildBeginDepends ();
  virtual void buildBeginPreDepends ();
  virtual void buildPriority (const std::string& );
  virtual void buildInstalledSize (const std::string& );
  virtual void buildMaintainer (const std::string& );
  virtual void buildArchitecture (const std::string& );
  virtual void buildInstallSize (const std::string& );
  virtual void buildInstallMD5 (unsigned char const[16]);
  virtual void buildSourceMD5 (unsigned char const[16]);
  virtual void buildBeginRecommends ();
  virtual void buildBeginSuggests ();
  virtual void buildBeginReplaces ();
  virtual void buildBeginConflicts ();
  virtual void buildBeginProvides ();
  virtual void buildBeginBuildDepends ();
  virtual void buildBeginBinary ();
  virtual void buildDescription (const std::string& );
  virtual void buildSourceName (const std::string& );
  virtual void buildSourceNameVersion (const std::string& );
  virtual void buildPackageListAndNode ();
  virtual void buildPackageListOrNode (const std::string& );
  virtual void buildPackageListOperator (PackageSpecification::_operators const &);
  virtual void buildPackageListOperatorVersion (const std::string& );

  unsigned int timestamp;
  std::string version;
  std::string parse_mirror;
};

#endif /* SETUP_INIDBBUILDER_H */
