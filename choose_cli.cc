/*
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#include "choose_cli.h"

#include "LogSingleton.h"
#include "resource.h"
#include "package_db.h"
#include "package_meta.h"

#include "getopt++/StringArrayOption.h"

#include <set>

static StringArrayOption DeletePackageOption ('x', "remove-packages", IDS_HELPTEXT_REMOVE_PACKAGES);
static StringArrayOption DeleteCategoryOption ('c', "remove-categories", IDS_HELPTEXT_REMOVE_CATEGORIES);
static StringArrayOption PackageOption ('P', "packages", IDS_HELPTEXT_PACKAGES);
static StringArrayOption CategoryOption ('C', "categories", IDS_HELPTEXT_CATEGORIES);

bool hasManualSelections = false;

static void
parseNames (std::set<std::string> &parsed, std::string &option)
{
  std::string tname;

  /* Split up the packages listed in the option.  */
  std::string::size_type loc = option.find (",", 0);
  while (loc != std::string::npos)
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

static void
validatePackageNames (std::set<std::string> &names)
{
  packagedb db;
  for (std::set<std::string>::iterator n = names.begin();
       n != names.end();
       ++n)
    {
      if (db.packages.find(*n) == db.packages.end())
        {
          Log(LOG_PLAIN) << "Package '" << *n << "' not found." << endLog;
        }
    }
}

bool
isManuallyWanted(packagemeta &pkg, packageversion &version)
{
  static bool parsed_yet = false;
  static std::map<std::string, std::string> parsed_names;
  hasManualSelections |= parsed_names.size ();
  static std::set<std::string> parsed_categories;
  hasManualSelections |= parsed_categories.size ();
  bool bReturn = false;

  /* First time through, we parse all the names out from the
    option string and store them away in an STL set.  */
  if (!parsed_yet)
  {
    std::vector<std::string> packages_options = PackageOption;
    std::vector<std::string> categories_options = CategoryOption;

    std::set<std::string> items;
    for (std::vector<std::string>::iterator n = packages_options.begin ();
                n != packages_options.end (); ++n)
      {
        parseNames (items, *n);
      }

    std::set<std::string> packages;
    /* Separate any 'package=version' into package and version parts */
    for (std::set<std::string>::iterator n = items.begin();
         n != items.end();
         ++n)
      {
        std::string package;
        std::string version;
        std::string::size_type loc = n->find ("=", 0);
        if (loc != std::string::npos)
          {
            package = n->substr(0, loc);
            version = n->substr(loc+1);
          }
        else
          {
            package = *n;
            version = "";
          }
        Log (LOG_BABBLE) << "package: " << package << " version: " << version << endLog;
        parsed_names[package] = version;
        packages.insert(package);
      }

    validatePackageNames (packages);

    for (std::vector<std::string>::iterator n = categories_options.begin ();
                n != categories_options.end (); ++n)
      {
        parseNames (parsed_categories, *n);
      }
    parsed_yet = true;
  }

  /* Once we've already parsed the option string, just do
    a lookup in the cache of already-parsed names.  */
  std::map<std::string, std::string>::iterator i = parsed_names.find(pkg.name);
  if (i != parsed_names.end())
    {
      bReturn = true;

      /* Wanted version is unspecified */
      version = packageversion();

      /* ... unless a version was explicitly specified */
      std::string v = i->second;
      if (!v.empty())
        {
          const packageversion *pv = pkg.findVersion(v);
          if (pv)
            version = *pv;
          else
            Log (LOG_PLAIN) << "package: " << pkg.name << " version: " << v << " not found" << endLog;
        }
    }

  /* If we didn't select the package manually, did we select any
     of the categories it is in? */
  if (!bReturn && parsed_categories.size ())
    {
      std::set<std::string, casecompare_lt_op>::iterator curcat;
      for (curcat = pkg.categories.begin (); curcat != pkg.categories.end (); curcat++)
        if (parsed_categories.find (*curcat) != parsed_categories.end ())
          {
            Log (LOG_BABBLE) << "Found category " << *curcat << " in package " << pkg.name << endLog;
            version = packageversion();
            bReturn = true;
          }
    }

  if (bReturn)
    Log (LOG_BABBLE) << "Added manual package " << pkg.name << endLog;
  return bReturn;
}

bool
isManuallyDeleted(packagemeta &pkg)
{
  static bool parsed_yet = false;
  static std::set<std::string> parsed_delete;
  hasManualSelections |= parsed_delete.size ();
  static std::set<std::string> parsed_delete_categories;
  hasManualSelections |= parsed_delete_categories.size ();
  bool bReturn = false;

  /* First time through, we parse all the names out from the
    option string and store them away in an STL set.  */
  if (!parsed_yet)
  {
    std::vector<std::string> delete_options   = DeletePackageOption;
    std::vector<std::string> categories_options = DeleteCategoryOption;
    for (std::vector<std::string>::iterator n = delete_options.begin ();
                n != delete_options.end (); ++n)
      {
        parseNames (parsed_delete, *n);
      }
    validatePackageNames (parsed_delete);
    for (std::vector<std::string>::iterator n = categories_options.begin ();
                n != categories_options.end (); ++n)
      {
        parseNames (parsed_delete_categories, *n);
      }
    parsed_yet = true;
  }

  /* Once we've already parsed the option string, just do
    a lookup in the cache of already-parsed names.  */
  bReturn = parsed_delete.find(pkg.name) != parsed_delete.end();

  /* If we didn't select the package manually, did we select any
     of the categories it is in? */
  if (!bReturn && parsed_delete_categories.size ())
    {
      std::set<std::string, casecompare_lt_op>::iterator curcat;
      for (curcat = pkg.categories.begin (); curcat != pkg.categories.end (); curcat++)
        if (parsed_delete_categories.find (*curcat) != parsed_delete_categories.end ())
          {
            Log (LOG_BABBLE) << "Found category " << *curcat << " in package " << pkg.name << endLog;
            bReturn = true;
          }
    }

  if (bReturn)
    Log (LOG_BABBLE) << "Deleted manual package " << pkg.name << endLog;
  return bReturn;
}
