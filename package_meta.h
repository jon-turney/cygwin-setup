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
class category;

/* Required to parse this completely */
#include <set>
#include "PackageTrust.h"
#include "package_version.h"
#include "package_message.h"

typedef std::pair<const std::string, std::vector<packagemeta *> > Category;

/* NOTE: A packagemeta without 1 packageversion is invalid! */
class packagemeta
{
public:
  static void ScanDownloadedFiles (bool);
  packagemeta (packagemeta const &);
  packagemeta (const std::string& pkgname)
  : name (pkgname), key(pkgname), user_picked (false),
    architecture (), priority(), visited_(false)
  {
  }

  ~packagemeta ();

  void add_version (packageversion &);
  void set_installed (packageversion &);
  void visited(bool const &);
  bool visited() const;
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
  int set_requirements (trusts deftrust, size_t depth);
  // explicit separation for generic programming.
  int set_requirements (trusts deftrust) 
    { return set_requirements (deftrust, 0); }
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

  const std::string SDesc () const;
  const std::string LDesc () const;

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
  /* which one is listed as "prev" in our available packages db */
  packageversion prev;
  /* ditto for current - stable */
  packageversion curr;
  /* and finally the experimental version */
  packageversion exp;
  /* Now for the user stuff :] */
  /* What version does the user want ? */
  packageversion desired;

  /* What platform is this for ? 
   * i386 - linux i386
   * cygwin - cygwin for 32 bit MS Windows 
   * All - no binary code, or a version for every platform
   */
  std::string architecture;
  /* What priority does this package have?
   * TODO: this should be linked into a list of priorities.
   */
  std::string priority;

  packagemessage message;

  /* can one or more versions be installed? */
  bool accessible () const;
  bool sourceAccessible() const;

  void logSelectionStatus() const;
  void logAllVersions() const;

protected:
  packagemeta &operator= (packagemeta const &);
private:
  std::string trustLabel(packageversion const &) const;
  bool visited_;
};

#endif /* SETUP_PACKAGE_META_H */
