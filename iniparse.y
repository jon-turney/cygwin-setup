%{
/*
 * Copyright (c) 2000, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

/* Parse the setup.ini files.  inilex.l provides the tokens for this. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "win32.h"
#include "package_db.h"
#include "category.h"
#include "ini.h"
#include "iniparse.h"
#include "filemanip.h"

extern "C" int yyerror (char const *s, ...);
int yylex ();

#include "port.h"

#include "package_meta.h"
#include "package_version.h"
#include "cygpackage.h"

#define YYERROR_VERBOSE 1
#define YYINITDEPTH 1000
/*#define YYDEBUG 1*/

static packagemeta *cp = 0;
extern unsigned int setup_timestamp;
extern char *setup_version;
extern int yylineno;

char * parse_mirror = 0;
static cygpackage *cpv = 0;
static int trust;

void add_correct_version();
void process_src (cygpackage *, packagesource &src, char *, char*, char* = 0);
%}

%token STRING
%token SETUP_TIMESTAMP SETUP_VERSION PACKAGEVERSION INSTALL SOURCE SDESC LDESC
%token CATEGORY REQUIRES
%token APATH PPATH INCLUDE_SETUP EXCLUDE_PACKAGE DOWNLOAD_URL
%token T_PREV T_CURR T_TEST T_UNKNOWN
%token MD5

%%

whole_file
 : setup_headers packages
 ;

setup_headers
 : setup_header setup_headers
 | /* empty */
 ;

setup_header
 : SETUP_TIMESTAMP STRING '\n' { setup_timestamp = strtoul ($2, 0, 0); }
 | SETUP_VERSION STRING '\n' { setup_version = new char [strlen($2) + 1];strcpy (setup_version, $2); }
 | '\n'
 | error { yyerror ("unrecognized line in setup.ini headers (do you have the latest setup?)"); } '\n'
 ;

packages
 : package packages
 | /* empty */
 ;

package
 : '@' STRING '\n'		{packagedb db; cp = &db.packages.registerbykey($2); cpv = new cygpackage ($2); trust = TRUST_CURR;}
   lines
 ;

lines
 : lines '\n' simple_line
 | simple_line
 ;

simple_line
 : PACKAGEVERSION STRING	{ cpv->set_canonical_version ($2); 
   				  add_correct_version ();}
 | SDESC STRING			{ cpv->set_sdesc ($2); }
 | LDESC STRING			{ cpv->set_ldesc ($2); }
 | CATEGORY categories
 | REQUIRES requires
 | INSTALL STRING STRING MD5    { process_src (cpv, cpv->bin, $2, $3, $4); }
 | INSTALL STRING STRING	{ process_src (cpv, cpv->bin, $2, $3); }
 | SOURCE STRING STRING MD5	{ process_src (cpv, cpv->src, $2, $3, $4); }
 | SOURCE STRING STRING		{ process_src (cpv, cpv->src, $2, $3); }
 | T_PREV			{ trust = TRUST_PREV; cpv = new cygpackage (cp->name); }
 | T_CURR			{ trust = TRUST_CURR; cpv = new cygpackage (cp->name); }
 | T_TEST			{ trust = TRUST_TEST; cpv = new cygpackage (cp->name); }
 | T_UNKNOWN			{ trust = TRUST_UNKNOWN; }
 | /* empty */
 | error  {	yyerror ("unrecognized line in package %s (do you have the latest setup?)", cp->name.cstr_oneuse());
		yyerrok;
	      }
 ;

requires
 : STRING			{ cpv->new_requirement($1); } requires
 | STRING			{ cpv->new_requirement($1); }
 ;

categories
 : STRING			{ packagedb db; cp->add_category (db.categories.registerbykey ($1));
 				} categories
 | STRING			{ packagedb db; cp->add_category (db.categories.registerbykey ($1)); }
 ;

%%

void
add_correct_version()
{
  int merged = 0;
  for (size_t n = 1; !merged && n <= cp->versions.number (); n++)
      if (!cp->versions[n]->Canonical_version().casecompare(cpv->Canonical_version()))
      {
	/* ASSUMPTIONS:
	   categories and requires are consistent for the same version across
	   all mirrors
	   */
	/* Copy the binary mirror across if this site claims to have an install */
	if (cpv->bin.sites.number ())
	  cp->versions[n]->bin.sites.registerbykey (cpv->bin.sites[1]->key);
	/* Ditto for src */
	if (cpv->src.sites.number ())
	  cp->versions[n]->src.sites.registerbykey (cpv->src.sites[1]->key);
	/* Copy the descriptions across */
	if (cpv->SDesc ().size() && !cp->versions[n]->SDesc ().size())
	  cp->versions[n]->set_sdesc (cpv->SDesc ());
	if (cpv->LDesc ().size() && !cp->versions[n]->LDesc ().size())
	  cp->versions[n]->set_ldesc (cpv->LDesc ());
	cpv = (cygpackage *)cp->versions[n];
	merged = 1;
      }
    if (!merged)
    cp->add_version (*cpv);
  /* trust setting */
  switch (trust)
  {
    case TRUST_CURR:
      if (cp->currtimestamp < setup_timestamp)
      {
	cp->currtimestamp = setup_timestamp;
	cp->curr = cpv;
      }
    break;
    case TRUST_PREV:
    if (cp->prevtimestamp < setup_timestamp)
    {
        cp->prevtimestamp = setup_timestamp;
	  cp->prev = cpv;
    }
    break;
    case TRUST_TEST:
    if (cp->exptimestamp < setup_timestamp)
    {
        cp->exptimestamp = setup_timestamp;
	cp->exp = cpv;
    }
    break;
  }
}

void
process_src (cygpackage *cpv, packagesource &src, char *path, char*size, char*md5 )
{ 
  if (!cpv->Canonical_version ().size())
    {
      fileparse f;
        if (parse_filename (path, f))
	  {
	    cpv->set_canonical_version (f.ver);
	    add_correct_version ();
	  }
    }
  
  if (!src.size)
    {
      src.size = atoi(size);
      src.set_canonical (path);
    }
  if (md5 && !src.md5.isSet())
    src.md5.set((unsigned char *)md5);
  src.sites.registerbykey (parse_mirror);
}
