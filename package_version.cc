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

/* this is the parent class for all package operations. 
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "package_version.h"
#include "package_db.h"
#include "package_meta.h"
#include "state.h"
#include "resource.h"

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
  String const Name(){return String();}
  String const Vendor_version() {return String();}
  String const Package_version() {return String();}
  String const Canonical_version() {return String();}
  void setCanonicalVersion (String const &) {}
  package_status_t Status (){return package_notinstalled;}
  package_type_t Type () {return package_binary;}
  String const getfirstfile () {return String();}
  String const getnextfile () {return String();}
  String const SDesc () {return String();}
  void set_sdesc (String const &) {}
  String const LDesc () {return String();}
  void set_ldesc (String const &) {}
  void uninstall (){}
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
  int t = data->Name ().casecompare (rhs.data->Name());
  if (t < 0)
    return true;
  else if (t > 0)
    return false;
  else if (data->Canonical_version().casecompare (rhs.data->Canonical_version()) < 0)
    return true;
  return false;
}

String const 
packageversion::Name () const
{
  return data->Name ();
}

String const
packageversion::Canonical_version() const
{
  return data->Canonical_version();
}

void
packageversion::setCanonicalVersion (String const &ver)
{
  data->setCanonicalVersion (ver);
}

String const
packageversion::getfirstfile ()
{
  return data->getfirstfile ();
}

String const
packageversion::getnextfile ()
{
  return data->getnextfile ();
}

String const
packageversion::SDesc () const
{
  return data->SDesc ();
}

void
packageversion::set_sdesc (String const &sdesc)
{
  data->set_sdesc (sdesc);
}

String const
packageversion::LDesc () const
{
  return data->LDesc ();
}

void
packageversion::set_ldesc (String const &ldesc)
{
  data->set_ldesc (ldesc);
}

packageversion
packageversion::sourcePackage()
{
  return data->sourcePackage();
}

PackageSpecification &
packageversion::sourcePackageSpecification ()
{
  return data->sourcePackageSpecification ();
}

void
packageversion::setSourcePackageSpecification (PackageSpecification const &spec)
{
  data->setSourcePackageSpecification(spec);
}

vector <vector <PackageSpecification *> *> *
packageversion::depends()
{
  return &data->depends;
}

vector <vector <PackageSpecification *> *> *
packageversion::predepends()
{
      return &data->predepends;
}

vector <vector <PackageSpecification *> *> *
packageversion::recommends()
{
      return &data->recommends;
}

vector <vector <PackageSpecification *> *> *
packageversion::suggests()
{
      return &data->suggests;
}

vector <vector <PackageSpecification *> *> *
packageversion::replaces()
{
      return &data->replaces;
}

vector <vector <PackageSpecification *> *> *
packageversion::conflicts()
{
      return &data->conflicts;
}

vector <vector <PackageSpecification *> *> *
packageversion::provides()
{
      return &data->provides;
}


bool
packageversion::picked () const
{
  return data->picked;
}

void 
packageversion::pick (bool aBool)
{
  data->picked = aBool;
}

bool
packageversion::changeRequested () const
{
  return data->changeRequested ();
}

void
packageversion::uninstall ()
{
 data->uninstall ();
}

packagesource *
packageversion::source ()
{
  return &data->source;
}

bool
packageversion::accessible() const
{
  return data->accessible();
}

/* the parent data class */
  
_packageversion::_packageversion ():picked (false), references (0)
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

bool
_packageversion::accessible() const
{
  return ((::source != IDC_SOURCE_CWD) && source.sites.number ()) ||
    source.Cached ();
}

bool
_packageversion::changeRequested ()
{
  return (picked || sourcePackage().picked());
}
