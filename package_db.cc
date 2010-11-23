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

/* this is the package database class.
 * It lists all known packages, including custom ones, ones from a mirror and
 * installed ones.
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <algorithm>
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include "io_stream.h"
#include "compress.h"

#include "filemanip.h"

#include "package_version.h"
#include "cygpackage.h"
#include "package_db.h"
#include "package_meta.h"
#include "Exception.h"
#include "Generic.h"

using namespace std;

packagedb::packagedb ()
{
  io_stream *db = 0;
  if (!installeddbread)
    {
      /* no parameters. Read in the local installation database. */
      db = io_stream::open ("cygfile:///etc/setup/installed.db", "rt", 0);
      installeddbread = 1;
      if (!db)
	return;
      /* flush_local_db_package_data */
      char line[1000], pkgname[1000], inst[1000];
      int instsz;

      if (db->gets (line, 1000))
	{
	  int dbver;
	  sscanf (line, "%s %d", pkgname, &instsz);
	  if (!strcasecmp (pkgname, "INSTALLED.DB") && instsz == 2)
	    dbver = 2;
	  else
	    dbver = 1;
	  delete db;
	  db = 0;
	  /* Later versions may not use installed.db other than to record the version. */
	  if (dbver == 1 || dbver == 2)
	    {
	      db =
		io_stream::open ("cygfile:///etc/setup/installed.db", "rt", 0);
	      if (dbver == 2)
		db->gets (line, 1000);
	      while (db->gets (line, 1000))
		{
		  int parseable;
		  int ign;
		  pkgname[0] = '\0';
		  inst[0] = '\0';

		  sscanf (line, "%s %s %d", pkgname, inst, &ign);

		  if (pkgname[0] == '\0' || inst[0] == '\0')
			continue;

		  fileparse f;
		  parseable = parse_filename (inst, f);
		  if (!parseable)
		    continue;

		  packagemeta *pkg = findBinary (PackageSpecification(pkgname));
		  if (!pkg)
		    {
		      pkg = new packagemeta (pkgname, inst);
		      packages.push_back (pkg);
		      /* we should install a new handler then not check this...
		       */
		      //if (!pkg)
		      //die badly
		    }

		  packageversion binary = 
		    cygpackage::createInstance (pkgname, inst, f.ver,
	    					package_installed,
	    					package_binary);

		  pkg->add_version (binary);
		  pkg->set_installed (binary);
		  pkg->desired = pkg->installed;
		}
	      delete db;
	      db = 0;
	    }
	  else
	    // unknown dbversion
	    exit (1);
	}
    }
}

int
packagedb::flush ()
{
  /* naive approach - just dump the lot */
  char const *odbn = "cygfile:///etc/setup/installed.db";
  char const *ndbn = "cygfile:///etc/setup/installed.db.new";

  io_stream::mkpath_p (PATH_TO_FILE, ndbn, 0755);

  io_stream *ndb = io_stream::open (ndbn, "wb", 0644);

  // XXX if this failed, try removing any existing .new database?
  if (!ndb)
    return errno ? errno : 1;

  ndb->write ("INSTALLED.DB 2\n", strlen ("INSTALLED.DB 2\n"));
  for (vector <packagemeta *>::iterator i = packages.begin ();
       i != packages.end (); ++i)
    {
      packagemeta & pkgm = **i;
      if (pkgm.installed)
	{
	  /* size here is irrelevant - as we can assume that this install source
	   * no longer exists, and it does not correlate to used disk space
	   * also note that we are writing a fictional install source 
	   * to keep cygcheck happy.               
	   */
	  std::string line;
	  line = pkgm.name + " " + pkgm.name + "-" + 
	    std::string(pkgm.installed.Canonical_version()) + ".tar.bz2 0\n";
	  ndb->write (line.c_str(), line.size());
	}
    }

  delete ndb;

  io_stream::remove (odbn);

  if (io_stream::move (ndbn, odbn))
    return errno ? errno : 1;
  return 0;
}

packagemeta *
packagedb::findBinary (PackageSpecification const &spec) const
{
  for (vector <packagemeta *>::iterator n = packages.begin ();
       n != packages.end (); ++n)
    {
      packagemeta & pkgm = **n;
      for (set<packageversion>::iterator i=pkgm.versions.begin();
	  i != pkgm.versions.end(); ++i)
	if (spec.satisfies (*i))
	  return &pkgm;
    }
  return NULL;
}

packagemeta *
packagedb::findSource (PackageSpecification const &spec) const
{
  for (vector <packagemeta *>::iterator n=sourcePackages.begin();
       n != sourcePackages.end(); ++n)
    {
      for (set<packageversion>::iterator i = (*n)->versions.begin();
	   i != (*n)->versions.end(); ++i)
	if (spec.satisfies (*i))
	  return *n;
    }
  return NULL;
}

/* static members */

int
  packagedb::installeddbread =
  0;
vector < packagemeta * > packagedb::packages;
packagedb::categoriesType
  packagedb::categories;
vector <packagemeta *> packagedb::sourcePackages;
PackageDBActions
  packagedb::task =
  PackageDB_Install;
std::vector <packagemeta *> 
packagedb::dependencyOrderedPackages;

#include "LogSingleton.h"
#include <stack>

class
ConnectedLoopFinder
{
  public:
  ConnectedLoopFinder();
  void doIt();
  packagedb db;
  size_t visited;
  std::vector<size_t> visitOrder;
  size_t visit (size_t const nodeToVisit);
  std::stack<size_t> nodesInStronglyConnectedComponent;
};

ConnectedLoopFinder::ConnectedLoopFinder() : visited(0)
{
  for (size_t counter = 0; counter < db.packages.size(); ++counter)
    visitOrder.push_back(0);
}

void
ConnectedLoopFinder::doIt()
{
  /* XXX this could be done useing a class to hold both the visitedInIteration and the package
   * meta reference. Then we could use a range, not an int loop. 
   */
  /* We have to expect dependency loops.  These loops break the topological
     sorting which would be a result of the below algorithm looking for
     strongly connected components in a directed graph.  Unfortunately it's
     not possible to order a directed graph with loops topologially.
     UGLY, but we really have to make sure that "base-cygwin" runs first,
     whatever the dependency loops come up with in future.
     FIXME: Find another depnendecy mechanism which ensures this without
            hardcoding. */
  for (size_t i = 0; i < db.packages.size(); ++i)
    {
      packagemeta &pkg (*db.packages[i]);
      if (pkg.installed && casecompare (pkg.name, "base-cygwin") == 0)
	{
	  visit (i);
	  break;
	}
    }
  for (size_t i = 0; i < db.packages.size(); ++i)
    {
      packagemeta &pkg (*db.packages[i]);
      if (pkg.installed && !visitOrder[i])
	visit (i);
    }
  log (LOG_BABBLE) << "Visited: " << visited << " nodes out of "
                   << db.packages.size() << " while creating dependency order."
                   << endLog;
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

size_t
ConnectedLoopFinder::visit(size_t const nodeToVisit)
{
  if (!db.packages[nodeToVisit]->installed)
    /* Can't visit this node, and it is not less than any visted node */
    return db.packages.size() + 1;
  ++visited;
  visitOrder[nodeToVisit] = visited;

  size_t minimumVisitId = visited;
  nodesInStronglyConnectedComponent.push(nodeToVisit);

  vector <vector <PackageSpecification *> *>::iterator dp = db.packages[nodeToVisit]->installed.depends ()->begin();
  /* walk through each and clause (a link in the graph) */
  while (dp != db.packages[nodeToVisit]->installed.depends ()->end())
    {
      /* check each or clause for an installed match */
      vector <PackageSpecification *>::iterator i =
	find_if ((*dp)->begin(), (*dp)->end(), checkForInstalled);
      if (i != (*dp)->end())
	{
	  /* we found an installed ok package */
	  /* visit it if needed */
	  /* UGLY. Need to refactor. iterators in the outer would help as we could simply
	   * vist the iterator
	   */
	   size_t nodeJustVisited = 0;
	   while (nodeJustVisited < db.packages.size() && casecompare(db.packages[nodeJustVisited]->name, (*i)->packageName())) 
	     ++nodeJustVisited;
	   if (nodeJustVisited == db.packages.size())
	     log (LOG_PLAIN) << "Search for package '" << (*i)->packageName() << "' failed." << endLog;
	   else
	   {
	     if (visitOrder[nodeJustVisited])
	       minimumVisitId = std::min (minimumVisitId, visitOrder[nodeJustVisited]);
	     else
	       minimumVisitId = std::min (minimumVisitId, visit (nodeJustVisited));
	   }
	  /* next and clause */
	  ++dp;
	  continue;
	}
	/* not installed or not available we ignore */
      ++dp;
    }
  
  if (minimumVisitId == visitOrder[nodeToVisit])
  {
    size_t popped;
    do {
      popped = nodesInStronglyConnectedComponent.top();
      nodesInStronglyConnectedComponent.pop();
      db.dependencyOrderedPackages.push_back(db.packages[popped]);
      /* mark as displayed in a connected component */
      visitOrder[popped] = db.packages.size() + 2;
    } while (popped != nodeToVisit);
  }
  
  return minimumVisitId;
}  

PackageDBConnectedIterator
packagedb::connectedBegin()
{
  if (!dependencyOrderedPackages.size())
    {
      ConnectedLoopFinder doMe;
      doMe.doIt();
      std::string s = "Dependency order of packages: ";
      
      for (std::vector<packagemeta *>::iterator i =
           dependencyOrderedPackages.begin();
           i != dependencyOrderedPackages.end(); ++i)
        s = s + (*i)->name + " ";
      log (LOG_BABBLE) << s << endLog;
    }
  return dependencyOrderedPackages.begin();
}

PackageDBConnectedIterator
packagedb::connectedEnd()
{
  return dependencyOrderedPackages.end();
}

void
packagedb::markUnVisited()
{
  for (vector <packagemeta *>::iterator n = packages.begin ();
       n != packages.end (); ++n)
    {
      packagemeta & pkgm = **n;
      pkgm.visited(false);
    }
}

void
packagedb::setExistence ()
{
  /* binary packages */
  /* Remove packages that are in the db, not installed, and have no 
     mirror info and are not cached for both binary and source packages. */
  vector <packagemeta *>::iterator i = packages.begin ();
  while (i != packages.end ())
    {
      packagemeta & pkg = **i;
      if (!pkg.installed && !pkg.accessible() && 
     !pkg.sourceAccessible() )
   {
   packagemeta *pkgm = *i;
   delete pkgm;
      i = packages.erase (i);
  }
      else
 ++i;
    }
#if 0
  /* remove any source packages which are not accessible */
  vector <packagemeta *>::iterator i = db.sourcePackages.begin();
  while (i != db.sourcePackages.end())
    {
      packagemeta & pkg = **i;
      if (!packageAccessible (pkg))
    {
   packagemeta *pkgm = *i;
   delete pkgm;
      i = db.sourcePackages.erase (i);
    }
      else
 ++i;
    }
#endif
}

void
packagedb::fillMissingCategory ()
{
  for_each(packages.begin(), packages.end(), visit_if(mem_fun(&packagemeta::setDefaultCategories), mem_fun(&packagemeta::hasNoCategories)));
  for_each(packages.begin(), packages.end(), mem_fun(&packagemeta::addToCategoryAll));
  for_each(packages.begin(), packages.end(), visit_if(mem_fun(&packagemeta::addToCategoryBase), mem_fun(&packagemeta::isManuallyWanted)));
}

