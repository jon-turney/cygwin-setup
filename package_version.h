/*
 * Copyright (c) 2001, 2003 Robert Collins.
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

#ifndef SETUP_PACKAGE_VERSION_H
#define SETUP_PACKAGE_VERSION_H

/* This is a package version abstrct class, that should be able to 
 * arbitrate acceess to cygwin binary packages, cygwin source package,
 * and the rpm and deb equivalents of the same.
 */

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
#include "PackageSpecification.h"
#include "PackageTrust.h"
#include "package_depends.h"

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
class packagemeta;

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

  const std::string Name () const; 
  const std::string Vendor_version () const;
  const std::string Package_version () const;
  const std::string Canonical_version () const;
  void setCanonicalVersion (const std::string& );
  package_type_t Type () const;
  const std::string SDesc () const;
  void set_sdesc (const std::string& );
  const std::string LDesc () const;
  void set_ldesc (const std::string& );
  packageversion sourcePackage () const;
  PackageSpecification & sourcePackageSpecification () const;
  void setSourcePackageSpecification (PackageSpecification const &);

  void setDepends(const PackageDepends);
  const PackageDepends depends() const;

  /* invariant: never null */
  packagesource *source() const; /* where can we source the file from */

  bool accessible () const;

  /* ensure that the depends clause is satisfied */
  int set_requirements (trusts deftrust, size_t depth = 0);

  /* utility function to compare package versions */
  static int compareVersions(const packageversion &a, const packageversion &b);

private:
  _packageversion *data; /* Invariant: * data is always valid */
};

class _packageversion
{
public:
  _packageversion();
  virtual ~_packageversion();
  /* for list inserts/mgmt. */
  std::string key;
  /* name is needed here, because if we are querying a file, the data may be embedded in
     the file */
  virtual const std::string Name () = 0;
  virtual const std::string Vendor_version () = 0;
  virtual const std::string Package_version () = 0;
  virtual const std::string Canonical_version () = 0;
  virtual void setCanonicalVersion (const std::string& ) = 0;
//  virtual package_stability_t Stability () = 0;
  virtual package_type_t Type () = 0;
  virtual const std::string SDesc () = 0;
  virtual void set_sdesc (const std::string& ) = 0;
  virtual const std::string LDesc () = 0;
  virtual void set_ldesc (const std::string& ) = 0;
  /* only semantically meaningful for binary packages */
  /* direct link to the source package for this binary */
  /* if multiple versions exist and the source doesn't discriminate
     then the most recent is used 
     */
  virtual packageversion sourcePackage ();
  virtual PackageSpecification & sourcePackageSpecification ();
  virtual void setSourcePackageSpecification (PackageSpecification const &);

  PackageDepends depends;

  packagesource source; /* where can we source the file from */

  virtual bool accessible () const;

  /* TODO: Implement me:
     static package_meta * scan_package (io_stream *);
   */
  size_t references;
protected:
  /* only meaningful for binary packages */
  PackageSpecification _sourcePackage;
  packageversion sourceVersion;
};

#endif /* SETUP_PACKAGE_VERSION_H */
