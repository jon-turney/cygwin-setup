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

#ifndef _PACKAGE_META_H_
#define _PACKAGE_META_H_

class packageversion;
class packagemeta;
class category;

/* Required to parse this completely */
#include <set>
#include "String++.h"
#include "category.h"
#include "PackageTrust.h"
#include "package_version.h"

/* NOTE: A packagemeta without 1 packageversion is invalid! */
class packagemeta
{
public:
  packagemeta (packagemeta const &);
  packagemeta (String const &pkgname):name (pkgname), key(pkgname), installed_from (),
  prevtimestamp (0), currtimestamp (0),
    exptimestamp (0), architecture (), priority()
  {
  }

  packagemeta (String const &pkgname,
	       String const &installedfrom):name (pkgname), key(pkgname),
	       installed_from (installedfrom),
    prevtimestamp (0), currtimestamp (0),
    exptimestamp (0), architecture (), priority()
  {
  };

  ~packagemeta ();

  void add_version (packageversion &);
  void set_installed (packageversion &);

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
  void set_action (packageversion const &default_version);
  void set_action (_actions, packageversion const & default_version);
  void uninstall ();
  int set_requirements (trusts deftrust = TRUST_CURR, size_t depth = 0);

  String action_caption ();
  packageversion trustp (trusts const t) const
  {
    return t == TRUST_PREV ? (prev ? prev : (curr ? curr : installed))
         : t == TRUST_CURR ? (curr ? curr : installed)
	 : exp ? exp : installed;
  }

  String name;			/* package name, like "cygwin" */
  String key;
  /* legacy variable used to output data for installed.db versions <= 2 */
  String installed_from;
  /* SDesc is global in theory, across all package versions. 
     LDesc is not: it can be different per version */
  String const SDesc () const;
  /* what categories does this package belong in. Note that if multiple versions
   * of a package disagree.... the first one read in will take precedence.
   */
  void add_category (String const &);
  std::set <String, String::caseless> categories;
  std::set <packageversion> versions;

  /* which one is installed. */
  packageversion installed;
  /* which one is listed as "prev" in our available packages db */
  packageversion prev;
  /* And what was the timestamp of the ini it was found from */
  unsigned int prevtimestamp;
  /* ditto for current - stable */
  packageversion curr;
  unsigned int currtimestamp;
  /* and finally the experimental version */
  packageversion exp;
  unsigned int exptimestamp;
  /* Now for the user stuff :] */
  /* What version does the user want ? */
  packageversion desired;

  /* What platform is this for ? 
   * i386 - linux i386
   * cygwin - cygwin for 32 bit MS Windows 
   * All - no binary code, or a version for every platform
   */
  String architecture;
  /* What priority does this package have?
   * TODO: this should be linked into a list of priorities.
   */
  String priority;

  /* can one or more versions be installed? */
  bool accessible () const;
  bool sourceAccessible() const;

  void logAllVersions() const;

protected:
  packagemeta &operator= (packagemeta const &);
private:
  String trustLabel(packageversion const &) const;
};

#endif /* _PACKAGE_META_H_ */
