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

#ifndef SETUP_PACKAGE_META_H
#define SETUP_PACKAGE_META_H

class packageversion;
class packagemeta;

#include <set>
#include <vector>
#include "PackageTrust.h"
#include "package_version.h"
#include "package_message.h"
#include "script.h"

typedef std::pair<const std::string, std::vector<packagemeta *> > Category;

/* NOTE: A packagemeta without 1 packageversion is invalid! */
class packagemeta
{
public:
  static void ScanDownloadedFiles (bool);
  packagemeta (packagemeta const &);
  packagemeta (const std::string& pkgname)
  : name (pkgname), key(pkgname), user_picked (false)
  {
  }

  ~packagemeta ();

  void add_version (packageversion &);
  void set_installed (packageversion &);
  void addToCategoryBase();
  bool hasNoCategories() const;
  void setDefaultCategories();
  void addToCategoryAll();

  class _actions
  {
  public:
    _actions ():_value (0) {};
    _actions (int aInt) {
    _value = aInt;
    if (_value < 0 ||  _value > 3)
      _value = 0;
    }
    _actions & operator ++ ();
    bool operator == (_actions const &rhs) { return _value == rhs._value; }
    bool operator != (_actions const &rhs) { return _value != rhs._value; }
    const char *caption ();
  private:
    int _value;
  };
  static const _actions Default_action;
  static const _actions Install_action;
  static const _actions Reinstall_action;
  static const _actions Uninstall_action;
  void set_action (trusts const t);
  void set_action (_actions, packageversion const & default_version);
  void uninstall ();

  void set_message (const std::string& message_id, const std::string& message_string)
  {
    message.set (message_id, message_string);
  }

  std::string action_caption () const;
  packageversion trustp (bool _default, trusts const t) const
  {
    /* If the user chose "test" and a "test" version is available, return it. */
    if (t == TRUST_TEST && exp)
      return exp;
    /* Are we looking for the default version and does the installed version
       have a higher version number than the "curr" package?  This means the
       user has installed a "test" version, or built her own version newer
       than "curr".  Rather than pulling the user back to "curr", we install
       "test" if a "test" version is available and the version number is higher,
       or we stick to "installed" if not.  This reflects the behaviour of
       `yum update' on Fedora. */
    if (_default && curr && installed
	&& packageversion::compareVersions (curr, installed) < 0)
      {
	if (exp && packageversion::compareVersions (installed, exp) < 0)
	  return exp;
	return installed;
      }
    /* Otherwise, if a "curr" version exists, return "curr". */
    if (curr)
      return curr;
    /* Otherwise return the installed version. */
    return installed;
  }

  std::string name;			/* package name, like "cygwin" */
  std::string key;

  /* true if package was selected on command-line. */
  bool isManuallyWanted() const;
  /* true if package was deleted on command-line. */
  bool isManuallyDeleted() const;
  /* SDesc is global in theory, across all package versions. 
     LDesc is not: it can be different per version */
  const std::string SDesc () const;
  /* what categories does this package belong in. Note that if multiple versions
   * of a package disagree.... the first one read in will take precedence.
   */
  void add_category (const std::string& );
  std::set <std::string, casecompare_lt_op> categories;
  const std::string getReadableCategoryList () const;
  std::set <packageversion> versions;

  /* Did the user already pick a version at least once? */
  bool user_picked;
  /* which one is installed. */
  packageversion installed;
  /* which one is listed as "current" (stable) in our available packages db */
  packageversion curr;
  /* ditto for "test" (experimental) */
  packageversion exp;
  /* Now for the user stuff :] */
  /* What version does the user want ? */
  packageversion desired;

  packagemessage message;

  /* can one or more versions be installed? */
  bool accessible () const;
  bool sourceAccessible() const;

  bool isBinary() const;

  void logSelectionStatus() const;
  void logAllVersions() const;

  void addScript(Script const &);
  std::vector <Script> &scripts();

protected:
  packagemeta &operator= (packagemeta const &);
private:
  std::string trustLabel(packageversion const &) const;
  std::vector <Script> scripts_;
};

#endif /* SETUP_PACKAGE_META_H */
