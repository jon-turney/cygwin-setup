/*
 * Copyright (c) 2001, Robert Collins.
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

/* This is a package version abstrct class, that should be able to 
 * arbitrate acceess to cygwin binary packages, cygwin source package,
 * and the rpm and deb equivalents of the same.
 */

#ifndef _PACKAGE_VERSION_H_
#define _PACKAGE_VERSION_H_

/* standard binary package metadata:
 * Name (ie mutt
 * Vendor Version (ie 2.5.1)
 * Package Version (ie 16)
 * Stability 
 * Files 
 */

/* For non installed files, this class can be populated via information about
 * what is available on the net, or by parsing a specific package file.
 * for installed packages, this class should represent what is currently installed,
 * - updated by what net metadata has about it.
 * i.e. the stability of this version will change simply because the net mirrors
 * now consider it old.
 */

class CategoryList;
class Dependency;

/* Required for parsing */
#include "package_source.h"
#include "String++.h"
#include "PackageSpecification.h"
#include <vector>

class Dependency
{
public:
  Dependency (String const &aPackage) : package (aPackage) {}
  Dependency (Dependency const &);
  Dependency * next;		/* the next package in this dependency list */
  PackageSpecification package; /* the package that is depended on */
}
 ;				/* Dependencies can be used for
				   recommended/required/related... */

typedef enum
{
  package_invalid,
  package_old,
  package_current,
  package_experimental
}
package_stability_t;

typedef enum
{
  package_notinstalled,
  package_installed
}
package_status_t;

typedef enum
{
  package_binary,
  package_source
}
package_type_t;

class packageversion
{
public:
  /* for list inserts/mgmt. */
  String key;
  /* name is needed here, because if we are querying a file, the data may be embedded in
     the file */
  virtual String const Name () = 0;
  virtual String const Vendor_version () = 0;
  virtual String const Package_version () = 0;
  virtual String const Canonical_version () = 0;
  virtual package_status_t Status () = 0;
//  virtual package_stability_t Stability () = 0;
  virtual package_type_t Type () = 0;
  /* TODO: we should probably return a metaclass - file name & path & size & type
     - ie doc/script/binary
   */
  virtual String const getfirstfile () = 0;
  virtual String const getnextfile () = 0;
  virtual String const SDesc () = 0;
  virtual void set_sdesc (String const &) = 0;
  virtual String const LDesc () = 0;
  virtual void set_ldesc (String const &) = 0;
  /* only semantically meaningful for binary packages */
  virtual PackageSpecification & sourcePackage ();
  virtual void setSourcePackage (PackageSpecification const &);
  
  /* FIXME: review this - these are UI variables, should be consistent across all
   * children package types
   */
  void new_requirement (String const &dependson)
  {
    if (!dependson.size())
        return;
    Dependency *dp = new Dependency (dependson);
      dp->next = required;
      required = dp;
  }
  Dependency *required;
  vector <vector <PackageSpecification *> *> depends, predepends, recommends,
  suggests, replaces, conflicts, provides;
  
  int srcpicked;		/* non zero if the source for this is required */
  int binpicked;		/* non zero if the binary is required  - 
				   This will also trigger reinstalled if it is set */


  virtual void uninstall () = 0;
  packagesource bin;
  packagesource src;

  packageversion ();
  virtual ~ packageversion ()
  {
  };

  /* TODO: Implement me:
     static package_meta * scan_package (io_stream *);
   */
protected:
  PackageSpecification _sourcePackage;
  

};

#endif /* _PACKAGE_VERSION_H_ */
