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

#include "category_list.h"

class cygpackage:public packageversion
{
public:
  virtual const char *Name ();
  virtual const char *Vendor_version ();
  virtual const char *Package_version ();
  virtual char const *Canonical_version ();
  virtual package_status_t Status ()
  {
    return status;
  };
  virtual package_type_t Type ()
  {
    return type;
  };
  void set_sdesc (char const *);
  void set_ldesc (char const *);
  virtual char const *SDesc ()
  {
    return sdesc;
  };
  virtual char const *LDesc ()
  {
    return ldesc;
  };
  virtual void uninstall ();


  /* pass the name of the package when constructing */
  cygpackage (const char *);
  cygpackage (const char *, const char *, size_t, const char *,
	      package_status_t, package_type_t);
  void set_canonical_version (char const *);


  virtual ~ cygpackage ();
  /* TODO: we should probably return a metaclass - file name & path & size & type
     - ie doc/script/binary
   */
  virtual const char *getfirstfile ();
  virtual const char *getnextfile ();
private:
  void destroy ();
  char *name;
  char *vendor;
  char *packagev;
  char *canonical;
  char *fn;
  char *sdesc, *ldesc;
  char getfilenamebuffer[_MAX_PATH];

//  package_stability_t stability;
  package_status_t status;
  package_type_t type;

  io_stream *listdata, *listfile;
  size_t filesize;
};

#endif /* _CYGPACKAGE_H_ */
