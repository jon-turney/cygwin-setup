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
void IniDBBuilder::buildPackageInstall (String const &, 
					String const &, unsigned char const[16]){}
void IniDBBuilder::buildPackageSource (String const &, 
				       String const &,unsigned char const[16]){}
void IniDBBuilder::buildPackageTrust (int){}
void IniDBBuilder::buildPackageRequirement (String const &){}
void IniDBBuilder::buildPackageCategory (String const &){}
