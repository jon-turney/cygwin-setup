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

#ifndef _INIDBBUILDERPACKAGE_H_
#define _INIDBBUILDERPACKAGE_H_

#include "IniDBBuilder.h"
class packagesource;
class packagemeta;
class cygpackage;

class IniDBBuilderPackage:public IniDBBuilder
{
public:
  virtual void buildTimestamp (String const &);
  virtual void buildVersion (String const &);
  virtual void buildPackage (String const &);
  virtual void buildPackageVersion (String const &);
  virtual void buildPackageSDesc (String const &);
  virtual void buildPackageLDesc (String const &);
  virtual void buildPackageInstall (String const &, String const &,
				    unsigned char const[16] = 0);
  virtual void buildPackageSource (String const &, String const &,
				   unsigned char const[16] = 0);
  virtual void buildPackageTrust (int);
  virtual void buildPackageRequirement (String const &);
  virtual void buildPackageCategory (String const &);

private:
  void add_correct_version();
  void process_src (packagesource &src, String const &, String const &, 
		    unsigned char const[16] = 0);
  packagemeta *cp;
  cygpackage *cpv;
  int trust;
};

#endif /* _INIDBBUILDERPACKAGE_H_ */
