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

/* This is a cygwin specific package class, that should be able to 
 * arbitrate acceess to cygwin binary packages amd cygwin source packages
 */

#ifndef _CYGPACKAGE_H_
#define _CYGPACKAGE_H_

#include "String++.h"

class cygpackage:public packageversion
{
public:
  virtual String const Name ();
  virtual String const Vendor_version ();
  virtual String const Package_version ();
  virtual String const Canonical_version ();
  virtual package_status_t Status ()
  {
    return status;
  };
  virtual package_type_t Type ()
  {
    return type;
  };
  virtual void set_sdesc (String const &);
  virtual void set_ldesc (String const &);
  virtual String const SDesc ()
  {
    return sdesc;
  };
  virtual String const LDesc ()
  {
    return ldesc;
  };
  virtual void uninstall ();


  /* pass the name of the package when constructing */
  cygpackage (String const &);
  cygpackage (String const &, String const &, size_t const, String const &,
	      package_status_t const, package_type_t const);
  void set_canonical_version (String const &);


  virtual ~ cygpackage ();
  /* TODO: we should probably return a metaclass - file name & path & size & type
     - ie doc/script/binary
   */
  virtual String const getfirstfile ();
  virtual String const getnextfile ();
private:
  void destroy ();
  String name;
  String vendor;
  String packagev;
  String canonical;
  String fn;
  String sdesc, ldesc;
  char getfilenamebuffer[_MAX_PATH];

//  package_stability_t stability;
  package_status_t status;
  package_type_t type;

  io_stream *listdata, *listfile;
  size_t filesize;
};

#endif /* _CYGPACKAGE_H_ */
