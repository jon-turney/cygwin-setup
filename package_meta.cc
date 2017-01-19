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

#if 0
static const char *cvsid = "\n%%% $Id$\n";
#endif

#include "package_meta.h"

#include <string>
#include <set>
using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include "getopt++/StringArrayOption.h"

#include "io_stream.h"
#include "compress.h"

#include "filemanip.h"
#include "LogSingleton.h"
/* io_stream needs a bit of tweaking to get rid of this. TODO */
#include "mount.h"
/* this goes at the same time */
#include "win32.h"


#include "script.h"

#include "package_version.h"
#include "cygpackage.h"
#include "package_db.h"

#include <algorithm>
#include "Generic.h"

using namespace std;

static StringArrayOption DeletePackageOption ('x', "remove-packages", "Specify packages to uninstall");
static StringArrayOption DeleteCategoryOption ('c', "remove-categories", "Specify categories to uninstall");
static StringArrayOption PackageOption ('P', "packages", "Specify packages to install");
static StringArrayOption CategoryOption ('C', "categories", "Specify entire categories to install");
bool hasManualSelections = 0;

/*****************/

const
  packagemeta::_actions
packagemeta::Default_action (0);
const
  packagemeta::_actions
packagemeta::Install_action (1);
const
  packagemeta::_actions
packagemeta::Reinstall_action (2);
const
  packagemeta::_actions
packagemeta::Uninstall_action (3);

char const *
packagemeta::_actions::caption ()
{
  switch (_value)
    {
    case 0:
      return "Default";
    case 1:
      return "Install";
    case 2:
      return "Reinstall";
    case 3:
      return "Uninstall";
    }
  // Pacify GCC: (all case options are checked above)
  return 0;
}

packagemeta::packagemeta (packagemeta const &rhs) :
  name (rhs.name), key (rhs.name),
  categories (rhs.categories), versions (rhs.versions),
  installed (rhs.installed), prev (rhs.prev),
  curr (rhs.curr),
  exp (rhs.exp),
  desired (rhs.desired),
  visited_(rhs.visited_)
{
  
}

packagemeta::_actions & packagemeta::_actions::operator++ ()
{
  ++_value;
  if (_value > 3)
    _value = 0;
  return *this;
}

template<class T> struct removeCategory : public unary_function<T, void>
{
  removeCategory(packagemeta *pkg) : _pkg (pkg) {}
  void operator() (T x) 
    {
      vector <packagemeta *> &aList = packagedb::categories[x]; 
      aList.erase (find (aList.begin(), aList.end(), _pkg));
    }
  packagemeta *_pkg;
};


packagemeta::~packagemeta()
{
  for_each (categories.begin (), categories.end (), removeCategory<std::string> (this));
  categories.clear ();
  versions.clear ();
}

void
packagemeta::add_version (packageversion & thepkg)
{
  /* todo: check return value */
  versions.insert (thepkg);
}

/* assumption: package thepkg is already in the metadata list. */
void
packagemeta::set_installed (packageversion & thepkg)
{
  set<packageversion>::const_iterator temp = versions.find (thepkg);
  if (temp != versions.end())
    installed = thepkg;
}

/* uninstall a package if it's installed */
void
packagemeta::uninstall ()
{
  if (installed)
    {
      /* this will need to be pushed down to the version, or even the source level
       * to allow differences between formats to be seamlessly managed
       * but for now: here is ok
       */
      set<string> dirs;
      string line = installed.getfirstfile ();

      while (line.size())
	{
          /* Insert the paths of all parent directories of line into dirs. */
          size_t idx = line.length();
          while ((idx = line.find_last_of('/', idx-1)) != string::npos)
          {
            string dir_path = line.substr(0, idx);
            bool was_new = dirs.insert(dir_path).second;
            /* If the path was already present in dirs, then all parent paths
             * must necessarily be present also, so don't do any further work.
             * */
            if (!was_new) break;
          }

	  std::string d = cygpath ("/" + line);
	  WCHAR wname[d.size () + 11]; /* Prefix + ".lnk". */
	  mklongpath (wname, d.c_str (), d.size () + 11);
	  DWORD dw = GetFileAttributesW (wname);
	  if (dw != INVALID_FILE_ATTRIBUTES
	      && !(dw & FILE_ATTRIBUTE_DIRECTORY))
	    {
	      Log (LOG_BABBLE) << "unlink " << d << endLog;
	      SetFileAttributesW (wname, dw & ~FILE_ATTRIBUTE_READONLY);
	      DeleteFileW (wname);
	    }
	  /* Check for Windows shortcut of same name. */
	  d += ".lnk";
	  wcscat (wname, L".lnk");
	  dw = GetFileAttributesW (wname);
	  if (dw != INVALID_FILE_ATTRIBUTES
	      && !(dw & FILE_ATTRIBUTE_DIRECTORY))
	    {
	      Log (LOG_BABBLE) << "unlink " << d << endLog;
	      SetFileAttributesW (wname, dw & ~FILE_ATTRIBUTE_READONLY);
	      DeleteFileW (wname);
	    }
	  line = installed.getnextfile ();
	}
      installed.uninstall ();

      /* An STL set maintains itself in sorted order. Thus, iterating over it
       * in reverse order will ensure we process directories depth-first. */
      set<string>::const_iterator it = dirs.end();
      while (it != dirs.begin())
      {
        it--;
        std::string d = cygpath("/" + *it);
	WCHAR wname[d.size () + 11];
	mklongpath (wname, d.c_str (), d.size () + 11);
	if (RemoveDirectoryW (wname))
	  Log (LOG_BABBLE) << "rmdir " << d << endLog;
      }
    }
  installed = packageversion();
}


void
packagemeta::add_category (const std::string& cat)
{
  if (categories.find (cat) != categories.end())
    return;
  /* add a new record for the package list */
  packagedb::categories[cat].push_back (this);
  categories.insert (cat);
}

struct StringConcatenator : public unary_function<const std::string, void>{
    StringConcatenator(std::string aString) : gap(aString){}
    void operator()(const std::string& aString) 
    {
      if (result.size() != 0)
        result += gap;
      result += aString;
    }
    std::string result;
    std::string gap;
};

const std::string
packagemeta::getReadableCategoryList () const
{
  return for_each(categories.begin(), categories.end(), 
    visit_if (
      StringConcatenator(", "), bind1st(not_equal_to<std::string>(), "All"))
              ).visitor.result;
}

static bool
hasSDesc(packageversion const &pkg)
{
  return pkg.SDesc().size();
}

static void
parseNames (std::set<string> &parsed, std::string &option)
{
  string tname;

  /* Split up the packages listed in the option.  */
  string::size_type loc = option.find (",", 0);
  while (loc != string::npos)
    {
      tname = option.substr (0, loc);
      option = option.substr (loc + 1);
      parsed.insert (tname);
      loc = option.find (",", 0);
    }

  /* At this point, no "," exists in option.  Don't add
     an empty string if the entire option was empty.  */
  if (option.length ())
    parsed.insert (option);
}

bool packagemeta::isManuallyWanted() const
{
  static bool parsed_yet = false;
  static std::set<string> parsed_names;
  hasManualSelections |= parsed_names.size ();
  static std::set<string> parsed_categories;
  hasManualSelections |= parsed_categories.size ();
  bool bReturn = false;

  /* First time through, we parse all the names out from the 
    option string and store them away in an STL set.  */
  if (!parsed_yet)
  {
    vector<string> packages_options = PackageOption;
    vector<string> categories_options = CategoryOption;
    for (vector<string>::iterator n = packages_options.begin ();
		n != packages_options.end (); ++n)
      {
	parseNames (parsed_names, *n);
      }
    for (vector<string>::iterator n = categories_options.begin ();
		n != categories_options.end (); ++n)
      {
	parseNames (parsed_categories, *n);
      }
    parsed_yet = true;
  }

  /* Once we've already parsed the option string, just do
    a lookup in the cache of already-parsed names.  */
  bReturn = parsed_names.find(name) != parsed_names.end();

  /* If we didn't select the package manually, did we select any 
     of the categories it is in? */
  if (!bReturn && parsed_categories.size ())
    {
      std::set<std::string, casecompare_lt_op>::iterator curcat;
      for (curcat = categories.begin (); curcat != categories.end (); curcat++)
	if (parsed_categories.find (*curcat) != parsed_categories.end ())
	  {
	    Log (LOG_BABBLE) << "Found category " << *curcat << " in package " << name << endLog;
	    bReturn = true;
	  }
    }
  
  if (bReturn)
    Log (LOG_BABBLE) << "Added manual package " << name << endLog;
  return bReturn;
}

bool packagemeta::isManuallyDeleted() const
{
  static bool parsed_yet = false;
  static std::set<string> parsed_delete;
  hasManualSelections |= parsed_delete.size ();
  static std::set<string> parsed_delete_categories;
  hasManualSelections |= parsed_delete_categories.size ();
  bool bReturn = false;

  /* First time through, we parse all the names out from the
    option string and store them away in an STL set.  */
  if (!parsed_yet)
  {
    vector<string> delete_options   = DeletePackageOption;
    vector<string> categories_options = DeleteCategoryOption;
    for (vector<string>::iterator n = delete_options.begin ();
		n != delete_options.end (); ++n)
      {
	parseNames (parsed_delete, *n);
      }
    for (vector<string>::iterator n = categories_options.begin ();
		n != categories_options.end (); ++n)
      {
	parseNames (parsed_delete_categories, *n);
      }
    parsed_yet = true;
  }

  /* Once we've already parsed the option string, just do
    a lookup in the cache of already-parsed names.  */
  bReturn = parsed_delete.find(name) != parsed_delete.end();

  /* If we didn't select the package manually, did we select any
     of the categories it is in? */
  if (!bReturn && parsed_delete_categories.size ())
    {
      std::set<std::string, casecompare_lt_op>::iterator curcat;
      for (curcat = categories.begin (); curcat != categories.end (); curcat++)
	if (parsed_delete_categories.find (*curcat) != parsed_delete_categories.end ())
	  {
	    Log (LOG_BABBLE) << "Found category " << *curcat << " in package " << name << endLog;
	    bReturn = true;
	  }
    }

  if (bReturn)
    Log (LOG_BABBLE) << "Deleted manual package " << name << endLog;
  return bReturn;
}

const std::string
packagemeta::SDesc () const
{
  set<packageversion>::iterator i = find_if (versions.begin(), versions.end(), hasSDesc);
  if (i == versions.end())
    return std::string();
  return i->SDesc ();
};

/* Return an appropriate caption given the current action. */
std::string 
packagemeta::action_caption () const
{
  if (!desired && installed)
    return "Uninstall";
  else if (!desired)
    return "Skip";
  else if (desired == installed && desired.picked())
    return packagedb::task == PackageDB_Install ? "Reinstall" : "Retrieve";
  else if (desired == installed && desired.sourcePackage() && desired.sourcePackage().picked())
    /* FIXME: Redo source should come up if the tarball is already present locally */
    return "Source";
  else if (desired == installed)	/* and neither src nor bin */
    return "Keep";
  else
    return desired.Canonical_version ();
}

/* Set the next action given a current action.  */
void
packagemeta::set_action (trusts const trust)
{
  set<packageversion>::iterator i;

  /* Keep the picked settings of the former desired version, if any, and make
     sure at least one of them is picked.  If both are unpicked, pick the
     binary version. */
  bool source_picked = desired && desired.sourcePackage().picked ();
  bool binary_picked = !desired || desired.picked () || !source_picked;

  /* If we're on "Keep" on the installed version, and the version is available,
     switch to "Reinstall". */
  if (desired && desired == installed && !desired.picked ()
      && desired.accessible ())
    {
      desired.pick (true, this);
      return;
    }

  if (!desired)
    {
      /* From "Uninstall" switch to the first version.  From "Skip" switch to
         the first version as well, unless the user picks for the first time.
	 In that case switch to the trustp version immediately. */
      if (installed || user_picked)
	i = versions.begin ();
      else
	for (i = versions.begin ();
	     i != versions.end () && *i != trustp (false, trust);
	     ++i)
	  ;
    }
  else
    {
      /* Otherwise switch to the next version. */
      for (i = versions.begin (); i != versions.end () && *i != desired; ++i)
	;
      ++i;
    }
  /* If there's another version in the list, switch to it, otherwise
     switch to "Uninstall". */
  if (i != versions.end ())
    {
      desired = *i;
      /* If the next version is the installed version, unpick it.  This will
	 have the desired effect to show the package in "Keep" mode.  See also
	 above for the code switching to "Reinstall". */
      desired.pick (desired != installed && binary_picked, this);
      desired.sourcePackage ().pick (desired.sourcePackage().accessible ()
				     && source_picked, NULL);
    }
  else
    desired = packageversion ();
  /* Memorize the fact that the user picked at least once. */
  if (!installed)
    user_picked = true;
}

int
packagemeta::set_requirements (trusts deftrust, size_t depth)
{
  if (visited())
    return 0;
  /* Only prevent further checks once we have been required by something else */
  if (depth)
    visited(true);
  int changed = 0;
  /* handle build-depends */
  if (depth == 0 && desired.sourcePackage ().picked())
    changed += desired.sourcePackage ().set_requirements (deftrust, depth + 1);
  if (!desired || (desired != installed && !desired.picked ()))
    /* uninstall || source only */
    return changed;

  return changed + desired.set_requirements (deftrust, depth);
}


// Set a particular type of action.
void
packagemeta::set_action (_actions action, packageversion const &default_version)
{
  if (action == Default_action)
    {
      if (installed
	  || categories.find ("Base") != categories.end ()
	  || categories.find ("Misc") != categories.end ())
	{
	  desired = default_version;
	  if (desired)
	    {
	      desired.pick (desired != installed, this);
	      desired.sourcePackage ().pick (false, NULL);
	    }
	}
      else
	desired = packageversion ();
      return;
    }
  else if (action == Install_action)
    {
      desired = default_version;
      if (desired)
	{
	  if (desired != installed)
	    if (desired.accessible ())
	      {
		user_picked = true;
		desired.pick (true, this);
		desired.sourcePackage ().pick (false, NULL);
	      }
	    else
	      {
		desired.pick (false, NULL);
		desired.sourcePackage ().pick (true, NULL);
	      }
	  else
	    {
	      desired.pick (false, NULL);
	      desired.sourcePackage ().pick (false, NULL);
	    }
	}
      return;
    }
  else if (action == Reinstall_action)
    {
      desired = installed;
      if (desired)
	{
	  desired.pick (true, this);
	  desired.sourcePackage ().pick (false, NULL);
	}
    }
  else if (action == Uninstall_action)
    {
      desired = packageversion ();
    }
}

bool
packagemeta::accessible () const
{
  for (set<packageversion>::iterator i=versions.begin();
       i != versions.end(); ++i)
    if (i->accessible())
      return true;
  return false;
}

bool
packagemeta::sourceAccessible () const
{
  for (set<packageversion>::iterator i=versions.begin();
       i != versions.end(); ++i)
    {
      packageversion bin=*i;
      if (bin.sourcePackage().accessible())
        return true;
    }
  return false;
}

void
packagemeta::logAllVersions () const
{
    for (set<packageversion>::iterator i = versions.begin();
	 i != versions.end(); ++i) 
      {
	Log (LOG_BABBLE) << "    [" << trustLabel(*i) <<
	  "] ver=" << i->Canonical_version() << endLog;
	if (i->depends()->size()) 
	{
	  std::ostream & logger = Log (LOG_BABBLE);
	  logger << "      depends=";
	  dumpAndList(i->depends(), logger);
	  logger << endLog;
	}
      }
#if 0
    Log (LOG_BABBLE) << "      inst=" << i->
      /* FIXME: Reinstate this code, but spit out all mirror sites */

      for (int t = 1; t < NTRUST; t++)
	{
	  if (pkg->info[t].install)
	    Log (LOG_BABBLE) << "     [%s] ver=%s\n"
		 "          inst=%s %d exists=%s\n"
		 "          src=%s %d exists=%s",
		 infos[t],
		 pkg->info[t].version ? : "(none)",
		 pkg->info[t].install ? : "(none)",
		 pkg->info[t].install_size,
		 (pkg->info[t].install_exists) ? "yes" : "no",
		 pkg->info[t].source ? : "(none)",
		 pkg->info[t].source_size,
		 (pkg->info[t].source_exists) ? "yes" : "no");
	}
#endif
}

std::string 
packagemeta::trustLabel(packageversion const &aVersion) const
{
    if (aVersion == prev)
	return "Prev";
    if (aVersion == curr)
	return "Curr";
    if (aVersion == exp)
	return "Test";
    return "Unknown";
}

void
packagemeta::visited(bool const &aBool)
{
  visited_ = aBool;
}

bool
packagemeta::visited() const
{
  return visited_;
}

void
packagemeta::logSelectionStatus() const
{
  packagemeta const & pkg = *this;
  const char *trust = ((pkg.desired == pkg.prev) ? "prev"
               : (pkg.desired == pkg.curr) ? "curr"
               : (pkg.desired == pkg.exp) ? "test" : "unknown");
  std::string action = pkg.action_caption ();
  const std::string installed =
   pkg.installed ? pkg.installed.Canonical_version () : "none";

  Log (LOG_BABBLE) << "[" << pkg.name << "] action=" << action << " trust=" << trust << " installed=" << installed << " src?=" << (pkg.desired && pkg.desired.sourcePackage().picked() ? "yes" : "no") << endLog;
  if (pkg.categories.size ())
    Log (LOG_BABBLE) << "     categories=" << for_each(pkg.categories.begin(), pkg.categories.end(), StringConcatenator(", ")).result << endLog;
#if 0
  if (pkg.desired.required())
  {
    /* List other packages this package depends on */
      Dependency *dp = pkg.desired->required;
    std::string requires = dp->package.serialise ();
    for (dp = dp->next; dp; dp = dp->next)
       requires += std::string (", ") + dp->package.serialise ();

   Log (LOG_BABBLE) << "     requires=" << requires;
    }
#endif
  pkg.logAllVersions();
}

void
packagemeta::ScanDownloadedFiles (bool mirror_mode)
{
  /* Look at every known package, in all the known mirror dirs,
   * and fill in the Cached attribute if it exists.
   */
  packagedb db;
  for (packagedb::packagecollection::iterator n = db.packages.begin ();
       n != db.packages.end (); ++n)
    {
      packagemeta & pkg = *(n->second);
      set<packageversion>::iterator i = pkg.versions.begin ();
      while (i != pkg.versions.end ())
	{
	  /* scan doesn't alter operator == for packageversions */
	  bool lazy_scan = mirror_mode
			   && (*i != pkg.installed
			       || pkg.installed == pkg.prev
			       || pkg.installed == pkg.curr
			       || pkg.installed == pkg.exp);
	  const_cast<packageversion &>(*i).scan (lazy_scan);
	  packageversion foo = *i;
	  packageversion pkgsrcver = foo.sourcePackage ();
	  pkgsrcver.scan (lazy_scan);

	  /* For local installs, if there is no src and no bin, the version
	   * is unavailable
	   */
	  if (!i->accessible () && !pkgsrcver.accessible ()
	      && *i != pkg.installed)
	    {
	      if (pkg.prev == *i)
		pkg.prev = packageversion ();
	      if (pkg.curr == *i)
		pkg.curr = packageversion ();
	      if (pkg.exp == *i)
		pkg.exp = packageversion ();
	      pkg.versions.erase (i++);
	      /* For now, leave the source version alone */
	    }
	  else
	    ++i;
	}
    }
    /* Don't explicity iterate through sources - any sources that aren't
       referenced are unselectable anyway.  */
}

void 
packagemeta::addToCategoryBase() 
{
  add_category ("Base");
}

bool
packagemeta::hasNoCategories() const
{
  return categories.size() == 0;
}

void
packagemeta::setDefaultCategories()
{
  add_category ("Misc");
}

void
packagemeta::addToCategoryAll()
{
  add_category ("All");
}
