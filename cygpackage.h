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

#ifndef SETUP_CYGPACKAGE_H
#define SETUP_CYGPACKAGE_H

/* This is a cygwin specific package class, that should be able to 
 * arbitrate acceess to cygwin binary packages amd cygwin source packages
 */

#include "String++.h"
/* for MAX_PATH */
#include "win32.h" 

#include "package_version.h"

class cygpackage:public _packageversion
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
  void setCanonicalVersion (String const &);


  virtual ~ cygpackage ();
  /* TODO: we should probably return a metaclass - file name & path & size & type
     - ie doc/script/binary
   */
  virtual String const getfirstfile ();
  virtual String const getnextfile ();

  /* pass the name of the package when constructing */
  static packageversion createInstance (String const &);
  static packageversion createInstance (String const &, String const &, 
					size_t const, String const &,
					package_status_t const, 
					package_type_t const);
private:
  cygpackage ();
  void destroy ();
  String name;
  String vendor;
  String packagev;
  String canonical;
  String fn;
  String sdesc, ldesc;
  char getfilenamebuffer[MAX_PATH];

//  package_stability_t stability;
  package_status_t status;
  package_type_t type;

  io_stream *listdata, *listfile;
  size_t filesize;
};

#endif /* SETUP_CYGPACKAGE_H */
