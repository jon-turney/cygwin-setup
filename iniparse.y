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

#include <stdlib.h>

#include "ini.h"
#include "iniparse.h"

static Package *cp;
static int trust;
extern unsigned int setup_timestamp;

#define cpt (cp->info+trust)

%}

%token STRING
%token SETUP_TIMESTAMP VERSION INSTALL SOURCE SDESC LDESC
%token T_PREV T_CURR T_TEST

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
 ;

packages
 : package packages
 | /* empty */
 ;

package
 : '@' STRING '\n'		{ new_package($2); }
   lines
 | '\n'
 ;

lines
 : simple_line '\n' lines
 | simple_line
 ;

simple_line
 : VERSION STRING		{ cpt->version = $2; }
 | SDESC STRING			{ cp->sdesc = $2; }
 | LDESC STRING			{ cp->ldesc = $2; }
 | INSTALL STRING STRING	{ cpt->install = $2;
				  cpt->install_size = atoi($3); }
 | SOURCE STRING STRING		{ cpt->source = $2;
				  cpt->source_size = atoi($3); }
 | T_PREV			{ trust = TRUST_PREV; }
 | T_CURR			{ trust = TRUST_CURR; }
 | T_TEST			{ trust = TRUST_TEST; }
 | /* empty */
 ;

%%

Package *package = 0;
int npackages = 0;
static int maxpackages = 0;

Package *
new_package (char *name)
{
  if (npackages >= maxpackages)
    {
      maxpackages += 10;
      if (package)
	package = (Package *) realloc (package, maxpackages * sizeof (Package));
      else
	package = (Package *) malloc (maxpackages * sizeof (Package));
    }
  cp = package + npackages;
  npackages ++;

  memset (cp, 0, sizeof (Package));
  cp->name = name;

  trust = TRUST_CURR;

  return cp;
}
