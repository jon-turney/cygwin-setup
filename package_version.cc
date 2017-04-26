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

/* this is the parent class for all package operations. 
 */

#include "package_version.h"
#include "package_db.h"
#include "package_meta.h"
#include "LogSingleton.h"
#include "state.h"
#include "resource.h"
#include <algorithm>
#include "csu_util/version_compare.h"

using namespace std;

/* a default class to avoid special casing empty packageversions */
  
/* TODO place into the class header */
class _defaultversion : public _packageversion
{
public:
  _defaultversion()
    {
      // never try to free me!
      ++references;
    }
  const std::string Name(){return std::string();}
  const std::string Vendor_version() {return std::string();}
  const std::string Package_version() {return std::string();}
  const std::string Canonical_version() {return std::string();}
  void setCanonicalVersion (const std::string& ) {}
  package_type_t Type () {return package_binary;}
  const std::string SDesc () {return std::string();}
  void set_sdesc (const std::string& ) {}
  const std::string LDesc () {return std::string();}
  void set_ldesc (const std::string& ) {}
};
static _defaultversion defaultversion;

/* the wrapper class */
packageversion::packageversion() : data (&defaultversion)
{
  ++data->references;
}

/* Create from an actual package */
packageversion::packageversion (_packageversion *pkg)
{
  if (pkg)
    data = pkg;
  else
    data = &defaultversion;
  ++data->references;
}

packageversion::packageversion (packageversion const &existing) : 
data(existing.data)
{
  ++data->references;
}

packageversion::~packageversion() 
{
  if (--data->references == 0)
    delete data;
}

packageversion &
packageversion::operator= (packageversion const &rhs)
{
  ++rhs.data->references;
  if (--data->references == 0)
    delete data;
  data = rhs.data;
  return *this;
}

bool
packageversion::operator ! () const
{
  return !data->Name().size();
}

packageversion::operator bool () const
{
  return data->Name().size();
}

bool
packageversion::operator == (packageversion const &rhs) const
{
  if (this == &rhs || data == rhs.data)
    return true;
  else
    return data->Name () == rhs.data->Name() && data->Canonical_version () == rhs.data->Canonical_version();
}

bool
packageversion::operator != (packageversion const &rhs) const
{
  return ! (*this == rhs);
}

bool
packageversion::operator < (packageversion const &rhs) const
{
  int t = casecompare(data->Name(), rhs.data->Name());
  if (t < 0)
    return true;
  else if (t > 0)
    return false;
  else if (casecompare (data->Canonical_version(), rhs.data->Canonical_version()) < 0)
    return true;
  return false;
}

const std::string 
packageversion::Name () const
{
  return data->Name ();
}

const std::string
packageversion::Vendor_version() const
{
  return data->Vendor_version();
}

const std::string
packageversion::Package_version() const
{
  return data->Package_version();
}

const std::string
packageversion::Canonical_version() const
{
  return data->Canonical_version();
}

void
packageversion::setCanonicalVersion (const std::string& ver)
{
  data->setCanonicalVersion (ver);
}

package_type_t
packageversion::Type () const
{
  return data->Type ();
}

const std::string
packageversion::SDesc () const
{
  return data->SDesc ();
}

void
packageversion::set_sdesc (const std::string& sdesc)
{
  data->set_sdesc (sdesc);
}

const std::string
packageversion::LDesc () const
{
  return data->LDesc ();
}

void
packageversion::set_ldesc (const std::string& ldesc)
{
  data->set_ldesc (ldesc);
}

packageversion
packageversion::sourcePackage() const
{
  return data->sourcePackage();
}

PackageSpecification &
packageversion::sourcePackageSpecification () const
{
  return data->sourcePackageSpecification ();
}

void
packageversion::setSourcePackageSpecification (PackageSpecification const &spec)
{
  data->setSourcePackageSpecification(spec);
}

PackageDepends *
packageversion::depends()
{
  return &data->depends;
}

const PackageDepends *
packageversion::depends() const
{
  return &data->depends;
}

packagesource *
packageversion::source () const
{
  return &data->source;
}

bool
packageversion::accessible() const
{
  return data->accessible();
}


int
packageversion::compareVersions(const packageversion &a, const packageversion &b)
{
  /* Compare Vendor_version */
  int comparison = version_compare(a.Vendor_version(), b.Vendor_version());
 
#if DEBUG
  Log (LOG_BABBLE) << "vendor version comparison " << a.Vendor_version() << " and " << b.Vendor_version() << ", result was " << comparison << endLog;
#endif

  if (comparison != 0)
    {
      return comparison;
    }

  /* Vendor_version are tied, compare Package_version */
#if DEBUG
  Log (LOG_BABBLE) <<  "package version comparison " << a.Package_version() << " and " << b.Package_version() << ", result was " << comparison << endLog;
#endif

  comparison = version_compare(a.Package_version(), b.Package_version());
  return comparison;
}

/* the parent data class */

_packageversion::_packageversion ():references (0)
{
}

_packageversion::~_packageversion ()
{
}

PackageSpecification &
_packageversion::sourcePackageSpecification ()
{
  return _sourcePackage;
}

void
_packageversion::setSourcePackageSpecification (PackageSpecification const &spec)
{
  _sourcePackage = spec;
}

packageversion
_packageversion::sourcePackage ()
{
  if (!sourceVersion)
    {
      packagedb db;
      packagemeta * pkg;
      pkg = db.findSource (_sourcePackage);
      /* no valid source meta available, just return the default
	 (blank) package version 
	 */
      if (!pkg)
	return sourceVersion;
      set<packageversion>::iterator i=pkg->versions.begin();
      while (i != pkg->versions.end())
	{
	  packageversion const & ver = * i;
          if (_sourcePackage.satisfies (ver))
	    sourceVersion = ver;
          ++i;
	}
    }
  return sourceVersion;
}

// is archive accessible
bool
_packageversion::accessible() const
{
  // cached ?
  if (source.Cached ())
    return true;
  // net access allowed?
  if (::source == IDC_SOURCE_LOCALDIR)
    return false;
  // retrievable ?
  if (source.sites.size() || source.Cached ())
    return true;
  // otherwise, not accessible
  return false;
}
