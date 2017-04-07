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

#include "libsolv.h"

#include "solv/solver.h"
#include "solv/solverdebug.h"
#include "solv/evr.h"

#include "LogSingleton.h"

// ---------------------------------------------------------------------------
// Utility functions for mapping between Operators and Relation Ids
// ---------------------------------------------------------------------------

static Id
Operator2RelId(PackageSpecification::_operators op)
{
  switch (op)
    {
    case PackageSpecification::Equals:
      return REL_EQ;
    case PackageSpecification::LessThan:
      return REL_LT;
    case PackageSpecification::MoreThan:
      return REL_GT;
    case PackageSpecification::LessThanEquals:
      return REL_LT | REL_EQ;
    case PackageSpecification::MoreThanEquals:
      return REL_GT | REL_EQ;
    }

  return 0;
}

static PackageSpecification::_operators
RelId2Operator(Id id)
{
  switch (id)
    {
    case REL_EQ:
      return PackageSpecification::Equals;
    case REL_LT:
      return PackageSpecification::LessThan;
    case REL_GT:
      return PackageSpecification::MoreThan;
    case REL_LT | REL_EQ:
      return PackageSpecification::LessThanEquals;
    case REL_GT | REL_EQ:
      return PackageSpecification::MoreThanEquals;
    }

  return PackageSpecification::Equals;
}

// ---------------------------------------------------------------------------
// implements class SolvableVersion
//
// a wrapper around a libsolv Solvable
// ---------------------------------------------------------------------------

const std::string
SolvableVersion::Name () const
{
  Solvable *solvable = pool_id2solvable(pool, id);
  return std::string(pool_id2str(pool, solvable->name));
}

const std::string
SolvableVersion::Canonical_version() const
{
  Solvable *solvable = pool_id2solvable(pool, id);
  return std::string(pool_id2str(pool, solvable->evr));
}

package_type_t
SolvableVersion::Type () const
{
  Solvable *solvable = pool_id2solvable(pool, id);
  if (solvable->arch == ARCH_SRC)
    return package_source;
  else
    return package_binary;
}

const PackageDepends
SolvableVersion::depends() const
{
  Solvable *solvable = pool_id2solvable(pool, id);

  Queue q;
  queue_init(&q);

  if (repo_lookup_idarray(solvable->repo, id, SOLVABLE_REQUIRES, &q))
    {
      // convert
      PackageDepends dep;

      for (int i = 0; i < q.count; i++)
        {
#ifdef DEBUG
          Log (LOG_PLAIN) << "dep " << std::hex << q.elements[i] << ": " << pool_dep2str(pool, q.elements[i]) << endLog;
#endif

          const char *name = pool_id2str(pool, q.elements[i]);
          PackageSpecification *spec = new PackageSpecification (name);

          if (ISRELDEP(id))
            {
              Reldep *rd = GETRELDEP(pool, id);
              spec->setOperator(RelId2Operator(rd->flags));
              spec->setVersion(pool_id2str(pool, rd->evr));
            }

          dep.push_back (spec);
        }

      queue_empty(&q);

      return dep;
    }

  // otherwise, return an empty depends list
  static PackageDepends empty_package;
  return empty_package;
}

const std::string
SolvableVersion::SDesc () const
{
  Solvable *solvable = pool_id2solvable(pool, id);
  const char *sdesc = repo_lookup_str(solvable->repo, id, SOLVABLE_SUMMARY);
  return sdesc;
}

SolvableVersion
SolvableVersion::sourcePackage () const
{
  if (!id)
    return SolvableVersion();

  // extract source package id
  Solvable *solvable = pool_id2solvable(pool, id);
  Id spkg_attr = pool_str2id(pool, "solvable:sourceid", 1);
  Id spkg_id = repo_lookup_id(solvable->repo, id, spkg_attr);

  // has no such attribute
  if (!spkg_id)
    return SolvableVersion();

  return SolvableVersion(spkg_id, pool);
}

packagesource *
SolvableVersion::source() const
{
  if (!id) {
    static packagesource empty_source = packagesource();
    return &empty_source;
  }

  Solvable *solvable = pool_id2solvable(pool, id);
  Id psrc_attr = pool_str2id(pool, "solvable:packagesource", 1);
  return (packagesource *)repo_lookup_num(solvable->repo, id, psrc_attr, 0);
}

bool
SolvableVersion::accessible () const
{
  // XXX: accessible if archive is locally available, or we know a mirror
  //
  // (This seems utterly pointless.  Packages which aren't locally available are
  // removed from the package list.  Packages we don't know a mirror for don't
  // appear in the packagelist.)
  return TRUE;
}

package_stability_t
SolvableVersion::Stability () const
{
  Solvable *solvable = pool_id2solvable(pool, id);
  Id stability_attr = pool_str2id(pool, "solvable:stability", 1);
  return (package_stability_t)repo_lookup_num(solvable->repo, id, stability_attr, TRUST_UNKNOWN);
}

bool
SolvableVersion::operator <(SolvableVersion const &rhs) const
{
  return (compareVersions(*this, rhs) < 0);
}

bool
SolvableVersion::operator ==(SolvableVersion const &rhs) const
{
  return (compareVersions(*this, rhs) == 0);
}

bool
SolvableVersion::operator !=(SolvableVersion const &rhs) const
{
  return (compareVersions(*this, rhs) != 0);
}

int
SolvableVersion::compareVersions(const SolvableVersion &a,
                                 const SolvableVersion &b)
{
  if (a.id == b.id)
    return 0;

  // if a and b are different, at least one of them has a pool
  Pool *pool = a.pool ? a.pool : b.pool;

  Solvable *sa = a.id ? pool_id2solvable(a.pool, a.id) : NULL;
  Solvable *sb = b.id ? pool_id2solvable(b.pool, b.id) : NULL;

  // empty versions compare as if their version is the empty string
  Id evra = sa ? sa->evr : pool_str2id(pool, "", 1);
  Id evrb = sb ? sb->evr : pool_str2id(pool, "", 1);

  return pool_evrcmp(pool, evra, evrb, EVRCMP_COMPARE);
}

// ---------------------------------------------------------------------------
// implements class SolverPool
//
// a simplified wrapper for libsolv
// ---------------------------------------------------------------------------

static
void debug_callback(Pool *pool, void *data, int type, const char *str)
{
  if (type & (SOLV_FATAL|SOLV_ERROR))
    LogPlainPrintf("libsolv: %s", str);
  else
    LogBabblePrintf("libsolv: %s", str);
}

SolverPool::SolverPool()
{
  /* create a pool */
  pool = pool_create();

  pool_setdebugcallback(pool, debug_callback, NULL);

  int level = 1;
#if DEBUG
  level = 3;
#endif
  pool_setdebuglevel(pool, level);

  /* create the repo to hold installed packages */
  SolvRepo *installed = getRepo("_installed");
  pool_set_installed(pool, installed->repo);
}

SolvRepo *
SolverPool::getRepo(const std::string &name, bool test)
{
  RepoList::iterator i = repos.find(name);
  if (i != repos.end())
    return i->second;

  /* create repo if not found */
  SolvRepo *r = new(SolvRepo);
  r->repo = repo_create(pool, name.c_str());

  /* create attribute store, with no local pool */
  r->data = repo_add_repodata(r->repo, 0);

  /* remember if this is a test stability repo */
  r->test = test;

  repos[name] = r;

  return r;
}

/*
  Helper function to convert a PackageDepends list to libsolv dependencies.
*/
Id
SolverPool::makedeps(Repo *repo, PackageDepends *requires)
{
  Id deps = 0;

  for (PackageDepends::iterator i = requires->begin();
       i != requires->end();
       i++)
    {
      Id name = pool_str2id(pool, (*i)->packageName().c_str(), 1);

      if ((*i)->version().size() == 0)
        {
          // no relation, so dependency is just on package name
          deps = repo_addid_dep(repo, deps, name, 0);
        }
      else
        {
          // otherwise, dependency is on package name with a version condition
          Id evr = pool_str2id(pool, (*i)->version().c_str(), 1);
          int rel = pool_rel2id(pool, name, evr, Operator2RelId((*i)->op()), 1);

          deps = repo_addid_dep(repo, deps, rel, 0);
        }
    }

  return deps;
}

SolvableVersion
SolverPool::addPackage(const std::string& pkgname, const addPackageData &pkgdata)
{
  std::string repoName = pkgdata.reponame;
  bool test = false;

  /* It's simplest to place test packages into a separate repo, and then
     arrange for that repo to be disabled, if we don't want to consider
     those packages */

  if (pkgdata.stability == TRUST_TEST)
    {
      repoName = pkgdata.reponame + "_test_";
      test = true;
    }

  SolvRepo *r = getRepo(repoName, test);
  Repo *repo = r->repo;

  /* create a solvable */
  Id s = repo_add_solvable(repo);
  Solvable *solvable = pool_id2solvable(pool, s);

  /* initialize solvable for this packageo/version/etc.  */
  solvable->name = pool_str2id(pool, pkgname.c_str(), 1);
  solvable->arch = (pkgdata.type == package_binary) ? ARCH_ANY : ARCH_SRC;
  solvable->evr = pool_str2id(repo->pool, pkgdata.version.c_str(), 1);
  solvable->vendor = pool_str2id(repo->pool, pkgdata.vendor.c_str(), 1);
  solvable->provides = repo_addid_dep(repo, solvable->provides, pool_rel2id(pool, solvable->name, solvable->evr, REL_EQ, 1), 0);
  if (pkgdata.requires)
    solvable->requires = makedeps(repo, pkgdata.requires);

  /* a solvable can also store arbitrary attributes not needed for dependency
     resolution, if we need them */

  Repodata *data = r->data;
  Id handle = s;
#if DEBUG
  Log (LOG_PLAIN) << "solvable " << s << " name " << pkgname << endLog;
#endif

  /* store short description attribute */
  repodata_set_str(data, handle, SOLVABLE_SUMMARY, pkgdata.sdesc.c_str());
  /* store long description attribute */
  repodata_set_str(data, handle, SOLVABLE_DESCRIPTION, pkgdata.ldesc.c_str());

  /* store source-package attribute */
  const std::string sname = pkgdata.spkg.packageName();
  if (!sname.empty())
    repodata_set_id(data, handle, SOLVABLE_SOURCENAME, pool_str2id(pool, sname.c_str(), 1));
  else
    repodata_set_void(data, handle, SOLVABLE_SOURCENAME);
  /* solvable:sourceevr may also be available from spkg but assumed to be same
     as evr for the moment */

  /* store source-package id */
  /* XXX: this assumes we create install package after source package and so can
     know that id */
  Id spkg_attr = pool_str2id(pool, "solvable:sourceid", 1);
  repodata_set_id(data, handle, spkg_attr, pkgdata.spkg_id.id);

  /* we could store packagesource information as attributes ...

     e.g.
       size     SOLVABLE_DOWNLOADSIZE
       pathname SOLVABLE_MEDIAFILE
       site     SOLVABLE_MEDIABASE
       checksum SOLVABLE_CHECKSUM

     ... but for the moment, we just store a pointer to a packagesource object
  */
  Id psrc_attr = pool_str2id(pool, "solvable:packagesource", 1);
  packagesource *psrc = new packagesource(pkgdata.archive);
  repodata_set_num(data, handle, psrc_attr, (intptr_t)psrc);

  /* store stability level attribute */
  Id stability_attr = pool_str2id(pool, "solvable:stability", 1);
  repodata_set_num(data, handle, stability_attr, pkgdata.stability);

#if 0
  repodata_internalize(data);

  /* debug: verify the attributes we've just set get retrieved correctly */
  SolvableVersion sv = SolvableVersion(s, pool);
  const std::string check_sdesc = sv.SDesc();
  if (pkgdata.sdesc.compare(check_sdesc) != 0) {
    Log (LOG_PLAIN) << pkgname << " has sdesc mismatch: '" << pkgdata.sdesc << "' and '"
                    << check_sdesc << "'" << endLog;
  }
  if (!sname.empty()) {
    SolvableVersion check_spkg = sv.sourcePackage();
    Solvable *check_spkg_solvable = pool_id2solvable(pool, check_spkg.id);
    std::string check_sname = pool_id2str(pool, check_spkg_solvable->name);
    if (sname.compare(check_sname) != 0) {
      Log (LOG_PLAIN) << pkgname << " has spkg mismatch: '" << pkgdata.spkg.packageName()
                      << "' and '" << check_sname << "'" << endLog;
    }
  }
  packagesource *check_archive = sv.source();
  if (check_archive != psrc)
    Log (LOG_PLAIN) << pkgname << " has archive mismatch: " << psrc
                    << " and " << check_archive << endLog;
  package_stability_t check_stability = sv.Stability();
  if (check_stability != pkgdata.stability) {
    Log (LOG_PLAIN) << pkgname << " has stability mismatch: " << pkgdata.stability
                    << " and " << check_stability << endLog;
  }
#endif

  return SolvableVersion(s, pool);
}

void
SolverPool::internalize()
{
  /* Make attribute data available to queries */
  for (RepoList::iterator i = repos.begin();
       i != repos.end();
       i++)
    {
      repodata_internalize(i->second->data);
    }
}
