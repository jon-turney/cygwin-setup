/*
 * Copyright (c) 2017 Jon Turney
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#ifndef LIBSOLV_H
#define LIBSOLV_H

#include "solv/pool.h"
#include "solv/repo.h"
#include "PackageSpecification.h"
#include "PackageTrust.h"
#include "package_source.h"
#include "package_depends.h"
#include <map>
#include <vector>

typedef trusts package_stability_t;

typedef enum
{
  package_binary,
  package_source
}
package_type_t;

// ---------------------------------------------------------------------------
// interface to class SolverVersion
//
// a wrapper around a libsolv Solvable
// ---------------------------------------------------------------------------

class SolverPool;

class SolvableVersion
{
 public:
  SolvableVersion() : id(0), pool(0) {};
  SolvableVersion(Id _id, Pool *_pool) : id(_id), pool(_pool) {};

  // converted to a bool, this is true if this isn't the result of the default
  // constructor (an 'empty' version, in some sense)
  explicit operator bool () const { return (id != 0); }

  const std::string Name () const;
  const std::string SDesc () const;
  // In setup-speak, 'Canonical' version means 'e:v-r', the non-decomposed version
  const std::string Canonical_version () const;
  const PackageDepends depends() const;
  bool accessible () const;
  package_type_t Type () const;
  package_stability_t Stability () const;
  // the associated source package, if this is a binary package
  SolvableVersion sourcePackage () const;
  // where this package archive can be obtained from
  packagesource *source() const;

  // utility function to compare package versions
  static int compareVersions(const SolvableVersion &a, const SolvableVersion &b);

  // comparison operators

  // these are somewhat necessary as otherwise we are compared as bool values
  bool operator == (SolvableVersion const &) const;
  bool operator != (SolvableVersion const &) const;

  // these are only well defined for versions of the same package
  bool operator < (SolvableVersion const &) const;
  bool operator <= (SolvableVersion const &) const;
  bool operator > (SolvableVersion const &) const;
  bool operator >= (SolvableVersion const &) const;

 private:
  Id id;
  Pool *pool;

  friend SolverPool;
};

// ---------------------------------------------------------------------------
// Helper class SolvRepo
//
// ---------------------------------------------------------------------------

class SolvRepo
{
public:
  Repo *repo;
  Repodata *data;
  bool test;
};

// ---------------------------------------------------------------------------
// interface to class SolverPool
//
// a simplified wrapper for libsolv
// ---------------------------------------------------------------------------

class SolverPool
{
public:
  SolverPool();
  SolvRepo *getRepo(const std::string &name, bool test = false);

  // Utility class for passing arguments to addPackage()
  class addPackageData
  {
  public:
    std::string reponame;
    std::string version;
    std::string vendor;
    std::string sdesc;
    std::string ldesc;
    package_stability_t stability;
    package_type_t type;
    packagesource archive;
    PackageSpecification spkg;
    SolvableVersion spkg_id;
    PackageDepends *requires;
  };

  SolvableVersion addPackage(const std::string& pkgname,
                             const addPackageData &pkgdata);

  void internalize(void);

private:
  Id makedeps(Repo *repo, PackageDepends *requires);
  Pool *pool;

  typedef std::map<std::string, SolvRepo *> RepoList;
  RepoList repos;
};


#endif // LIBSOLV_H
