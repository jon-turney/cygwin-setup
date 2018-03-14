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
#include "solv/solver.h"
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
class SolverSolution;

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
  // Return the dependency list
  const PackageDepends depends() const;
  // Return the obsoletes list
  const PackageDepends obsoletes() const;
  bool accessible () const;
  package_type_t Type () const;
  package_stability_t Stability () const;
  // the associated source package name, if this is a binary package
  const std::string sourcePackageName () const;
  // the associated source package, if this is a binary package
  SolvableVersion sourcePackage () const;
  // where this package archive can be obtained from
  packagesource *source() const;

  // fixup spkg_id
  void fixup_spkg_id(SolvableVersion spkg_id) const;

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

  void remove() const;

 private:
  Id id;
  Pool *pool;

  friend SolverPool;
  friend SolverSolution;

  const PackageDepends deplist(Id keyname) const;
  Id name_id () const;
};

// ---------------------------------------------------------------------------
// Helper class SolvRepo
//
// ---------------------------------------------------------------------------

class SolvRepo
{
public:
  typedef enum
  {
    priorityLow = 0,
    priorityNormal,
    priorityHigh,
  } priority_t;
  Repo *repo;
  Repodata *data;
  bool test;
  void setPriority(priority_t p) { repo->priority = p; }
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
  void clear();
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
    PackageDepends *obsoletes;
    PackageDepends *provides;
    PackageDepends *conflicts;
  };

  SolvableVersion addPackage(const std::string& pkgname,
                             const addPackageData &pkgdata);

  void internalize(void);
  void use_test_packages(bool use_test_packages);


private:
  void init();
  Id makedeps(Repo *repo, PackageDepends *requires);
  Pool *pool;

  typedef std::map<std::string, SolvRepo *> RepoList;
  RepoList repos;

  friend SolverSolution;
};


// ---------------------------------------------------------------------------
// interface to class SolverTaskQueue
//
// This is used to contain a set of package install/uninstall tasks selected via
// the UI to be passed to solver
// ---------------------------------------------------------------------------

class SolverTasks
{
 public:
  enum task
  {
    taskInstall,
    taskUninstall,
    taskReinstall,
    taskKeep,
    taskSkip,
    taskForceDistUpgrade,
  };
  void add(const SolvableVersion &v, task t)
  {
    tasks.push_back(taskList::value_type(v, t));
  };
  /* Create solver tasks corresponding to state of database */
  void setTasks();

 private:
  typedef std::vector<std::pair<const SolvableVersion, task>> taskList;
  taskList tasks;

  friend SolverSolution;
};

// ---------------------------------------------------------------------------
// SolverTransactionList
//
// a list of transactions output by the solver
// ---------------------------------------------------------------------------

class SolverTransaction
{
 public:
  typedef enum
  {
    transIgnore,
    transInstall,
    transErase,
  } transType;

  SolverTransaction(SolvableVersion version_, transType type_) :
  version(version_), type(type_) {};

  SolvableVersion version;
  transType type;
};

typedef std::vector<SolverTransaction> SolverTransactionList;

// ---------------------------------------------------------------------------
// interface to class SolverSolution
//
// A wrapper around the libsolv solver
// ---------------------------------------------------------------------------

class SolverSolution
{
 public:
  SolverSolution(SolverPool &_pool);
  ~SolverSolution();
  void clear();

  /* Reset package database to correspond to trans */
  void trans2db() const;

  /* Reset transaction list to correspond to package database */
  void db2trans();

  enum updateMode
  {
    keep,        // don't update
    updateBest,  // update to best version
    updateForce, // distupdate: downgrade if necessary to best version in repo
  };
  bool update(SolverTasks &tasks, updateMode update, bool use_test_packages);
  void augmentTasks(SolverTasks &tasks);
  void addSource(bool include_source);
  void applyDefaultProblemSolutions();
  std::string report() const;

  const SolverTransactionList &transactions() const;
  void dumpTransactionList() const;

 private:
  static SolverTransaction::transType type(Transaction *trans, int pos);
  bool solve();
  void tasksToJobs(SolverTasks &tasks, updateMode update, Queue &job);
  void solutionToTransactionList();

  SolverPool &pool;
  Solver *solv;
  Queue job;
  SolverTransactionList trans;
};

#endif // LIBSOLV_H
