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

#include "package_version.h"

class io_stream;

class cygpackage:public _packageversion
{
public:
  virtual const std::string Name ();
  virtual const std::string Vendor_version ();
  virtual const std::string Package_version ();
  virtual const std::string Canonical_version ();
  virtual package_stability_t Stability ()
  {
    return stability;
  }
  virtual void SetStability (package_stability_t newstability)
  {
    stability = newstability;
  }
  virtual package_type_t Type ()
  {
    return type;
  };
  virtual void set_sdesc (const std::string& );
  virtual void set_ldesc (const std::string& );
  virtual const std::string SDesc ()
  {
    return sdesc;
  };
  virtual const std::string LDesc ()
  {
    return ldesc;
  };

  /* pass the name of the package when constructing */
  void setCanonicalVersion (const std::string& );

  virtual ~ cygpackage ();

  /* pass the name of the package when constructing */
  static packageversion createInstance (const std::string& pkgname,
                                        const package_type_t type);

  static packageversion createInstance (const std::string& pkgname,
                                        const std::string& version,
					package_type_t const);

private:
  cygpackage ();
  std::string name;
  std::string vendor;
  std::string packagev;
  std::string canonical;
  std::string sdesc, ldesc;

  package_stability_t stability;
  package_type_t type;
};

#endif /* SETUP_CYGPACKAGE_H */
