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
 
/*Required for parsing */
#include "package_source.h"
#include "String++.h"
#include "PackageSpecification.h"
#include "PackageTrust.h"
#include <vector>

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

/* A wrapper class to be copied by value that
   references the same package.
   Nothing is virtual, because the wrapper cannot be inherited.
   However, as all the methods are implemented in the referenced
   _packageversion, that class allows virtual overriding.
   */

class _packageversion;

/* This class has pointer semantics 
   Specifically: a=b does not alter the value of *a.
   */
class packageversion
{
public:
  packageversion (); /* creates an empty packageversion */
  packageversion (_packageversion *); /* used when creating an instance */
  packageversion (packageversion const &);
  ~packageversion (); 
  packageversion &operator= (packageversion const &);
  bool operator ! () const; /* true if the package is invalid. (i.e.
			       uninitialised */
  operator bool () const; /* returns ! !() */
  bool operator == (packageversion const &) const; /* equality */
  bool operator != (packageversion const &) const;
  bool operator < (packageversion const &) const;
  bool operator <= (packageversion const &) const;
  bool operator > (packageversion const &) const;
  bool operator >= (packageversion const &) const;

  String const Name () const; 
  String const Vendor_version () const;
  String const Package_version () const;
  String const Canonical_version () const;
  void setCanonicalVersion (String const &);
  package_status_t Status () const;
  package_type_t Type () const;
  String const getfirstfile ();
  String const getnextfile ();
  String const SDesc () const;
  void set_sdesc (String const &);
  String const LDesc () const;
  void set_ldesc (String const &);
  packageversion sourcePackage ();
  PackageSpecification & sourcePackageSpecification ();
  void setSourcePackageSpecification (PackageSpecification const &);

  /* invariant: these never return NULL */
  vector <vector <PackageSpecification *> *> *depends(), *predepends(), 
  *recommends(), *suggests(), *replaces(), *conflicts(), *provides(), *binaries();

  bool picked() const;   /* true if this version is to be installed */
  void pick(bool); /* trigger an install/reinsall */
  /* a change - install/uninstall/reinstall/source install
        has been requested */
  bool changeRequested () const;

  void uninstall ();
  /* invariant: never null */
  packagesource *source(); /* where can we source the file from */
  /* invariant: never null */
  vector <packagesource> *sources(); /* expose the list of files.
					source() returns the 'default' file
					sources() allows managing multiple files
					in a single package
					*/

  bool accessible () const;

  /* ensure that the depends clause is satisfied */
  int set_requirements (trusts deftrust = TRUST_CURR, size_t depth = 0);
  
private:
  _packageversion *data; /* Invariant: * data is always valid */
};

class _packageversion
{
public:
  _packageversion();
  virtual ~_packageversion();
  /* for list inserts/mgmt. */
  String key;
  /* name is needed here, because if we are querying a file, the data may be embedded in
     the file */
  virtual String const Name () = 0;
  virtual String const Vendor_version () = 0;
  virtual String const Package_version () = 0;
  virtual String const Canonical_version () = 0;
  virtual void setCanonicalVersion (String const &) = 0;
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
  /* direct link to the source package for this binary */
  /* if multiple versions exist and the source doesn't discriminate
     then the most recent is used 
     */
  virtual packageversion sourcePackage ();
  virtual PackageSpecification & sourcePackageSpecification ();
  virtual void setSourcePackageSpecification (PackageSpecification const &);
  
  vector <vector <PackageSpecification *> *> depends, predepends, recommends,
  suggests, replaces, conflicts, provides, binaries;
  
  bool picked;	/* non zero if this version is to be installed */
		/* This will also trigger reinstalled if it is set */
  /* a change - install/uninstall/reinstall/source install
     has been requested */
  bool changeRequested ();


  virtual void uninstall () = 0;
  vector<packagesource> sources; /* where can we source the files from */

  bool accessible () const;

  /* TODO: Implement me:
     static package_meta * scan_package (io_stream *);
   */
  size_t references;
protected:
  /* only meaningful for binary packages */
  PackageSpecification _sourcePackage;
  packageversion sourceVersion;
};

#endif /* _PACKAGE_VERSION_H_ */
