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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "package_version.h"
#include "package_db.h"
#include "package_meta.h"
#include "LogSingleton.h"
#include "state.h"
#include "resource.h"
#include <algorithm>

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
  void pick(bool const &newValue){/* Ignore attempts to pick this!. Throw an exception here if you want to detect such attemtps instead */}
  virtual void addScript(Script const &) {}
  virtual std::vector <Script> &scripts() { scripts_.clear();  return scripts_;}
  virtual bool accessible () const {return false;}
  private:
    std::vector <Script> scripts_;
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
packageversion::sourcePackage() const
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

vector <vector <PackageSpecification *> *> *
packageversion::binaries()
{
      return &data->binaries;
}

bool
packageversion::picked () const
{
  return data->picked;
}

void 
packageversion::pick (bool aBool)
{
  data->pick(aBool);
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
  if (!data->sources.size())
    data->sources.push_back (packagesource());
  return &data->sources[0];
}

vector<packagesource> *
packageversion::sources ()
{
  return &data->sources;
}

bool
packageversion::accessible() const
{
  return data->accessible();
}

static bool
checkForInstalled (PackageSpecification *spec)
{
  packagedb db;
  packagemeta *required = db.findBinary (*spec);
  if (!required)
    return false;
  if (spec->satisfies (required->installed)
      && required->desired == required->installed )
    /* done, found a satisfactory installed version that will remain
       installed */
    return true;
  return false;
}

static bool
checkForUpgradeable (PackageSpecification *spec)
{
  packagedb db;
  packagemeta *required = db.findBinary (*spec);
  if (!required || !required->installed)
    return false;
  for (set <packageversion>::iterator i = required->versions.begin();
       i != required->versions.end(); ++i)
    if (spec->satisfies (*i))
      return true;
  return false;
}

static bool
checkForSatisfiable (PackageSpecification *spec)
{
  packagedb db;
  packagemeta *required = db.findBinary (*spec);
  if (!required)
    return false;
  for (set <packageversion>::iterator i = required->versions.begin();
       i != required->versions.end(); ++i)
    if (spec->satisfies (*i))
      return true;
  return false;
}

/* Convenience class for now */
class DependencyProcessor {
public:
  DependencyProcessor (trusts const &aTrust, int aDepth=0) : deftrust (aTrust), depth (aDepth) {}
  trusts const deftrust;
  size_t depth;
};

static int
select (DependencyProcessor &processor, packagemeta *required, packageversion const &aVersion)
{
  /* preserve source */
  bool sourceticked = required->desired.sourcePackage().picked();
  /* install this version */
  required->desired = aVersion;
  required->desired.pick (required->installed != required->desired);
  required->desired.sourcePackage().pick (sourceticked);
  /* does this requirement have requirements? */
  return required->set_requirements (processor.deftrust, processor.depth + 1);
}

static int
processOneDependency(trusts deftrust, size_t depth, PackageSpecification *spec)
{
  /* TODO: add this to a set of packages to be offered to meet the
     requirement. For now, simply set the install to the first
     satisfactory version. The user can step through the list if
     desired */
  packagedb db;
  packagemeta *required = db.findBinary (*spec);
  DependencyProcessor processor (deftrust, depth);

  packageversion trusted = required->trustp(deftrust);
  if (spec->satisfies (trusted)) {
      return select (processor,required,trusted);
  }

  log (LOG_TIMESTAMP) << "Warning, the default trust level for package "
    << trusted.Name() << " does not meet this specification " << *spec
    << endLog;
  
  set <packageversion>::iterator v;
  for (v = required->versions.begin();
    v != required->versions.end() && !spec->satisfies (*v); ++v);

  if (v == required->versions.end())
      /* assert ?! */
      return 0;
  
  return select (processor, required, *v);
}

int
packageversion::set_requirements (trusts deftrust, size_t depth)
{
  int changed = 0;
  vector <vector <PackageSpecification *> *>::iterator dp = depends ()->begin();
  /* cheap test for too much recursion */
  if (depth > 5)
    return changed;
  /* walk through each and clause */
  while (dp != depends ()->end())
    {
      /* three step:
	 1) is a satisfactory or clause installed?
	 2) is an unsatisfactory version of an or clause which has
	 a satisfactory version available installed?
	 3) is a satisfactory package available?
	 */
      /* check each or clause for an installed match */
      vector <PackageSpecification *>::iterator i =
	find_if ((*dp)->begin(), (*dp)->end(), checkForInstalled);
      if (i != (*dp)->end())
	{
	  /* we found an installed ok package */
	  /* next and clause */
	  ++dp;
	  continue;
	}
      /* check each or clause for an upgradeable version */
      i = find_if ((*dp)->begin(), (*dp)->end(), checkForUpgradeable);
      if (i != (*dp)->end())
	{
	  /* we found a package that can be up/downgraded to meet the
	     requirement. (*i is the packagespec that can be satisfied.)
	     */
	  ++dp;
	  changed += processOneDependency (deftrust, depth, *i) + 1;
	  continue;
	}
      /* check each or clause for an installable version */
      i = find_if ((*dp)->begin(), (*dp)->end(), checkForSatisfiable);
      if (i != (*dp)->end())
	{
	  /* we found a package that can be installed to meet the requirement */
	  ++dp;
	  changed += processOneDependency (deftrust, depth, *i) + 1;
	  continue;
	}
      ++dp;
    }
  return changed;
}

void
packageversion::addScript(Script const &aScript)
{
  return data->addScript (aScript);
}

std::vector <Script> &
packageversion::scripts()
{
  return data->scripts();
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
  bool cached (sources.size() > 0);
  for (vector<packagesource>::const_iterator i = sources.begin();
       i!=sources.end(); ++i)
    if (!i->Cached ())
      cached = false;
  if (cached) 
    return true;
  if (::source == IDC_SOURCE_CWD)
    return false;
  unsigned int retrievable = 0;
  for (vector<packagesource>::const_iterator i = sources.begin();
      i!=sources.end(); ++i)
    if (i->sites.size() || i->Cached ())
      retrievable += 1;
  return retrievable > 0;
}

bool
_packageversion::changeRequested ()
{
  return (picked || sourcePackage().picked());
}

void
_packageversion::addScript(Script const &aScript)
{
  scripts().push_back(aScript);
}

std::vector <Script> &
_packageversion::scripts()
{
  return scripts_;
}
