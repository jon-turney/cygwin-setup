/*
 * Copyright (c) 2002 Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <robertc@hotmail.com>
 *
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "ScanFindVisitor.h"
#include "filemanip.h"
#include "IniDBBuilder.h"

ScanFindVisitor::ScanFindVisitor(IniDBBuilder &aBuilder) : _Builder (aBuilder) {}
ScanFindVisitor::~ScanFindVisitor(){}

/* look for potential packages we can add to the in-memory package
 * database
 */
void
ScanFindVisitor::visitFile(String const &basePath, const WIN32_FIND_DATA *theFile)
{
  // Sanity check: Does the file look like a package ?
  fileparse f;
  if (!parse_filename (theFile->cFileName, f))
    return;

  // Sanity check: Zero length package files get thrown out.
  if (!(theFile->nFileSizeLow || theFile->nFileSizeHigh))
    return;

  // Build a new package called f.pkg
  _Builder.buildPackage (f.pkg);

  // Set the version we are bulding
  _Builder.buildPackageVersion (f.ver);

  // Add the file as a installable package
  if (!f.what.size())
    //assume binary
    _Builder.buildPackageInstall (basePath + theFile->cFileName, theFile->nFileSizeLow);
  else
    // patch or src, assume src until someone complains
    _Builder.buildPackageSource (basePath + theFile->cFileName, theFile->nFileSizeLow);
 
  // TODO: Review the below code. We may wish to reinstate it *somewhere*.  

#if 0
  /* Scan existing package list looking for a match between a known
     package and a tar archive on disk.
     While scanning, keep track of appropriate "holes" in the trust
     table where a tar file could be put if no known entry
     exists.

     We have 4 specific insertion points and one generic point.
     The generic point is in versioned order in the package version array.
     The specific points are
     *installed
     *prev
     *curr
     *exp.

     if the version number matches a version in the db,
     we simply add this as a mirror source to that version.
     If it matches no version, we add a new version to the db.

     Lastly if the version number does not matche one of installed/prev/current/exp
     AND we had to create a new version entry
     we apply the following heuristic:
     if there is no exp, we link this in exp.
     If there is an exp and this is higher, we link this in exp, and
     if there is no curr, bump what was in exp to curr. If there was a curr, we leave it be.
     if this is lower than exp, and there is no curr, link as curr. If there is a curr, leave it be.
     If this is lower than curr, and there is no prev, link as prev, if there is a prev, leave it be.

     Whilst this logic is potentially wrong from time to time, it guarantees that
     setup.ini defined stability won't be altered unintentially. An alternative is to
     mark setup.ini defined prev/curr/exp packages as such, when this algorithm, can
     get smarter.

     So, if setup.ini knows that ash-20010425-1.tar.gz is the current
     version and there is an ash-20010426-1.tar.gz in the current directory,
     the 20010426 version will be placed in the "test" slot, assuming that
     there is no test version listed in setup.ini. */

  int added = 0;
  for (size_t n = 1; n <= pkg->versions.number (); n++)
    {
      if (!f.ver.casecompare (pkg->versions[n]->Canonical_version ()))
        {
          if (f.what == String ())
            {
              //bin package
              pkg->versions[n]->bin.set_cached (String ("file://") + basePath + theFile->cFileName);
            }
          else if (f.what == "src")
            {
              //src package
              pkg->versions[n]->src.set_cached (String ("file://") + basePath + theFile->cFileName);
            }
          added = 1;
        }
    }
  if (!added)
    {
#if 0
      // Do we want old versions to show up
      packageversion *pv = new cygpackage (f.pkg);
      ((cygpackage *) pv)->set_canonical_version (f.ver);
      if (!f.what.size ())
        pv->bin.set_cached (String ("file://") + path);
      else
        // patch or src, assume src until someone complains
        pv->src.set_cached (String ("file://") + path);
      pkg->add_version (*pv);

#endif

      /* And now the hole finder */
#if 0
      if (!pkg->exp)
        pkg->exp = thenewver;
      else if (strcasecmp (f.ver, pkg->versions[n]->Canonicalversion ()) < 0)
        /* try curr */
        if (!pkg->curr)
          pkg->curr = thenewver;
        else if (strcasecmp (f.ver, pkg->versions[n]->Canonicalversion ()) <
                 0)
          /* try prev */
          if (!pkg->prev)
            pkg->prev = thenewver;
#endif
    }
#endif
}
