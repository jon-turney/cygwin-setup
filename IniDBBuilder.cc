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
void IniDBBuilder::buildTimestamp (String const &){}
void IniDBBuilder::buildVersion (String const &){}
void IniDBBuilder::buildPackage (String const &){}
void IniDBBuilder::buildPackageVersion (String const &){}
void IniDBBuilder::buildPackageSDesc (String const &){}
void IniDBBuilder::buildPackageLDesc (String const &){}
void IniDBBuilder::buildPackageInstall (String const &){}
void IniDBBuilder::buildPackageSource (String const &, String const &){}
void IniDBBuilder::buildPackageTrust (int){}
void IniDBBuilder::buildPackageCategory (String const &){}
void IniDBBuilder::buildBeginDepends (){}
void IniDBBuilder::buildBeginPreDepends (){}
void IniDBBuilder::buildPriority (String const &){}
void IniDBBuilder::buildInstalledSize (String const &){}
void IniDBBuilder::buildMaintainer (String const &){}
void IniDBBuilder::buildArchitecture (String const &){}
void IniDBBuilder::buildInstallSize (String const &){}
void IniDBBuilder::buildInstallMD5 (unsigned char const[16]){}
void IniDBBuilder::buildSourceMD5 (unsigned char const[16]){}
void IniDBBuilder::buildBeginRecommends (){}
void IniDBBuilder::buildBeginSuggests (){}
void IniDBBuilder::buildBeginReplaces (){}
void IniDBBuilder::buildBeginConflicts (){}
void IniDBBuilder::buildBeginProvides (){}
void IniDBBuilder::buildDescription (String const &){}
void IniDBBuilder::buildSourceName (String const &){}
void IniDBBuilder::buildSourceNameVersion (String const &){}
void IniDBBuilder::buildPackageListAndNode (){}
void IniDBBuilder::buildPackageListOrNode (String const &){}
void IniDBBuilder::buildPackageListOperator (PackageSpecification::_operators const &){}
void IniDBBuilder::buildPackageListOperatorVersion (String const &){}
