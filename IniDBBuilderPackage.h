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

#ifndef SETUP_INIDBBUILDERPACKAGE_H
#define SETUP_INIDBBUILDERPACKAGE_H

#include <vector>
#include <set>

#include "package_message.h"
#include "PackageTrust.h"
#include "String++.h"
#include "libsolv.h"

class IniParseFeedback;
class packagesource;

enum class hashType { none, md5, sha512 };

class IniDBBuilderPackage
{
public:
  IniDBBuilderPackage (IniParseFeedback const &);
  ~IniDBBuilderPackage ();

  void buildTimestamp (const std::string& );
  void buildVersion (const std::string& );
  const std::string buildMinimumVersion(const std::string &);
  void buildPackage (const std::string& );
  void buildPackageVersion (const std::string& );
  void buildPackageSDesc (const std::string& );
  void buildPackageLDesc (const std::string& );
  void buildPackageInstall (const std::string&, const std::string&,
                            char *, hashType);
  void buildPackageSource (const std::string&, const std::string&,
                           char *, hashType);

  void buildPackageTrust (trusts);
  void buildPackageCategory (const std::string& );

  void buildBeginDepends ();
  void buildBeginBuildDepends ();
  void buildBeginObsoletes ();
  void buildBeginProvides ();
  void buildBeginConflicts ();
  void buildMessage (const std::string&, const std::string&);
  void buildSourceName (const std::string& );
  void buildSourceNameVersion (const std::string& );
  void buildPackageListNode (const std::string& );
  void buildPackageListOperator (PackageSpecification::_operators const &);
  void buildPackageListOperatorVersion (const std::string& );
  void buildPackageReplaceVersionsList (const std::string& );

  void set_arch (const std::string& a) { arch = a; }
  void set_release (const std::string& rel) { release = rel; }

  // setup.ini header data
  unsigned int timestamp;
  std::string arch;
  std::string release;
  std::string version;
  std::string parse_mirror;

private:
  void process ();

  // package data
  std::string name;
  std::set <std::string, casecompare_lt_op> categories;
  std::string message_id;
  std::string message_string;
  PackageSpecification *currentSpec;
  PackageDepends *currentNodeList;
  PackageDepends dependsNodeList;
  PackageDepends obsoletesNodeList;
  PackageDepends providesNodeList;
  PackageDepends conflictsNodeList;
  SolverPool::addPackageData cbpv;
  std::set <std::string> replace_versions;

  IniParseFeedback const &_feedback;
};

#endif /* SETUP_INIDBBUILDERPACKAGE_H */
