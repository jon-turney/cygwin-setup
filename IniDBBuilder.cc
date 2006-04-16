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

#include "IniDBBuilder.h"

IniDBBuilder::~IniDBBuilder(){}
void IniDBBuilder::buildTimestamp (const std::string& ){}
void IniDBBuilder::buildVersion (const std::string& ){}
void IniDBBuilder::buildPackage (const std::string& ){}
void IniDBBuilder::buildPackageVersion (const std::string& ){}
void IniDBBuilder::buildPackageSDesc (const std::string& ){}
void IniDBBuilder::buildPackageLDesc (const std::string& ){}
void IniDBBuilder::buildPackageInstall (const std::string& ){}
void IniDBBuilder::buildPackageSource (const std::string&,
                                       const std::string&){}
void IniDBBuilder::buildSourceFile (unsigned char const[16],
                                    const std::string&, const std::string& ){}
void IniDBBuilder::buildPackageTrust (int){}
void IniDBBuilder::buildPackageCategory (const std::string& ){}
void IniDBBuilder::buildBeginDepends (){}
void IniDBBuilder::buildBeginPreDepends (){}
void IniDBBuilder::buildPriority (const std::string& ){}
void IniDBBuilder::buildInstalledSize (const std::string& ){}
void IniDBBuilder::buildMaintainer (const std::string& ){}
void IniDBBuilder::buildArchitecture (const std::string& ){}
void IniDBBuilder::buildInstallSize (const std::string& ){}
void IniDBBuilder::buildInstallMD5 (unsigned char const[16]){}
void IniDBBuilder::buildSourceMD5 (unsigned char const[16]){}
void IniDBBuilder::buildBeginRecommends (){}
void IniDBBuilder::buildBeginSuggests (){}
void IniDBBuilder::buildBeginReplaces (){}
void IniDBBuilder::buildBeginConflicts (){}
void IniDBBuilder::buildBeginProvides (){}
void IniDBBuilder::buildBeginBuildDepends (){}
void IniDBBuilder::buildBeginBinary (){}
void IniDBBuilder::buildDescription (const std::string& ){}
void IniDBBuilder::buildSourceName (const std::string& ){}
void IniDBBuilder::buildSourceNameVersion (const std::string& ){}
void IniDBBuilder::buildPackageListAndNode (){}
void IniDBBuilder::buildPackageListOrNode (const std::string& ){}
void IniDBBuilder::buildPackageListOperator (PackageSpecification::_operators const &){}
void IniDBBuilder::buildPackageListOperatorVersion (const std::string& ){}
