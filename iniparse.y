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
#include "category_list.h"
#include "ini.h"
#include "iniparse.h"
#include "filemanip.h"

extern "C" int yyerror (char *s, ...);
extern "C" int yylex ();

#include "port.h"

#define YYERROR_VERBOSE 1
/*#define YYDEBUG 1*/

static Package *cp;
static int trust;
extern unsigned int setup_timestamp;
extern char *setup_version;
extern int yylineno;

packagedb db;

#define cpt (cp->info+trust)

%}

%token STRING
%token SETUP_TIMESTAMP SETUP_VERSION VERSION INSTALL SOURCE SDESC LDESC
%token CATEGORY REQUIRES
%token APATH PPATH INCLUDE_SETUP EXCLUDE_PACKAGE DOWNLOAD_URL
%token T_PREV T_CURR T_TEST T_UNKNOWN

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
 | SETUP_VERSION STRING '\n' { setup_version = _strdup ($2); }
 | '\n'
 | error { yyerror ("unrecognized line in setup.ini headers (do you have the latest setup?)"); } '\n'
 ;

packages
 : package packages
 | /* empty */
 ;

package
 : '@' STRING '\n'		{ new_package($2); }
   lines
 ;

lines
 : simple_line '\n' lines
 | simple_line
 ;

simple_line
 : VERSION STRING		{ cpt->version = $2; }
 | SDESC STRING			{ cp->sdesc = $2; }
 | LDESC STRING			{ cp->ldesc = $2; }
 | CATEGORY categories
 | REQUIRES requires
 | INSTALL STRING STRING	{ cpt->install = $2;
				  cpt->install_size = atoi($3);
				  if (!cpt->version)
				    {
				      fileparse f;
				      if (parse_filename ($2, f))
					cpt->version = strdup (f.ver);
				    }
				}
 | SOURCE STRING STRING		{ cpt->source = $2;
				  cpt->source_size = atoi($3); }
 | T_PREV			{ trust = TRUST_PREV; }
 | T_CURR			{ trust = TRUST_CURR; }
 | T_TEST			{ trust = TRUST_TEST; }
 | EXCLUDE_PACKAGE		{ cp->exclude = EXCLUDE_BY_SETUP; }
 | T_UNKNOWN			{ trust = TRUST_UNKNOWN; }
 | /* empty */
 | error '\n' { yylineno --;
		yyerror ("unrecognized line in package %s (do you have the latest setup?)", cp->name);
		yylineno ++;
	      }
 ;

requires
 : STRING			{ new_requirement(cp, $1); } requires
 | STRING			{ new_requirement(cp, $1); }
 ;

categories
 : STRING			{ add_category (cp, db.categories.register_category ($1));
 				} categories
 | STRING			{ add_category (cp, db.categories.register_category ($1)); }
 ;

%%

Package *package = NULL;
int npackages = 0;
static int maxpackages = 0;

Package *
new_package (char *name)
{
  if (package == NULL)
    maxpackages = npackages = 0;
  if (npackages >= maxpackages)
    {
      maxpackages += 100;
      if (package)
	package = (Package *) realloc (package, (1 + maxpackages) * sizeof (Package));
      else
	package = (Package *) malloc ((1 + maxpackages) * sizeof (Package));
      memset (package + npackages, 0, (1 + maxpackages - npackages) * sizeof (Package));
    }
  cp = getpkgbyname (name);
  if (!cp)
    {
      cp = package + npackages;
      npackages++;
      cp->name = strdup (name);
      trust = TRUST_CURR;
    }

  return cp;
}

void
new_requirement (Package *package, char *dependson)
{
  Dependency *dp;
  if (!dependson)
    return;
  dp = (Dependency *) malloc (sizeof (Dependency));
  dp->next = cp->required;
  dp->package = dependson;
  cp->required = dp;
}

void
add_category (Package *pkg, Category *cat)
{
  /* add a new record for the package list */
  /* TODO: alpabetical inserts ? */
  Category *tempcat = new Category;
  tempcat->next = pkg->category;
  tempcat->name = cat->name;
  pkg->category = tempcat;

  CategoryPackage *templink = new CategoryPackage;
  templink->next = cat->packages;
  templink->pkgname = pkg->name;
  cat->packages = templink;
}
