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

/* This is a generic package class, that should be able to 
 * arbitrate acceess to cygwin binary packages, cygwin source package,
 * and the rpm and deb equivalents of the same.
 */

#ifndef _PACKAGE_H_
#define _PACKAGE_H_

/* standard binary package metadata:
 * Name (ie mutt
 * Vendor Version (ie 2.5.1)
 * Package Version (ie 16)
 * Stability 
 * Files 
 */

/* For non installed files, this class can be populated via information about
 * what is available on the net, or by parsing a specific package file.
 * for installed packages, this class should represent what is currently installed,
 * - updated by what net metadata has about it.
 * i.e. the stability of this version will change simply because the net mirrors
 * now consider it old.
 */

typedef enum
{
  package_invalid,
  package_old,
  package_current,
  package_experimental
}
package_stability_t;

typedef enum
{
  package_notinstalled,
  package_installed
}
package_status_t;

typedef enum
{
  package_binary,
  package_source
}
package_type_t;

class genericpackage
{
public:
  /* name is needed here, because if we are querying a file, the data may be embedded in
     the file */
  virtual const char *Name () = 0;
  virtual const char *Vendor_version () = 0;
  virtual const char *Package_version () = 0;
  virtual package_status_t Status () = 0;
//  virtual package_stability_t Stability () = 0;
  virtual package_type_t Type () = 0;
  /* TODO: we should probably return a metaclass - file name & path & size & type
     - ie doc/script/binary
   */
  virtual const char *getfirstfile () = 0;
  virtual const char *getnextfile () = 0;

    virtual ~ genericpackage ()
  {
  };

private:
};

#endif /* _PACKAGE_H_ */
