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
#include "ini.h"
#include "iniparse.h"
#include "PackageTrust.h"

extern int yyerror (String  const &s);
int yylex ();

#include "port.h"

#include "IniDBBuilder.h"

#define YYERROR_VERBOSE 1
#define YYINITDEPTH 1000
/*#define YYDEBUG 1*/

IniDBBuilder *iniBuilder;
extern int yylineno;

void add_correct_version();
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
 : SETUP_TIMESTAMP STRING '\n' { iniBuilder->buildTimestamp ($2); }
 | SETUP_VERSION STRING '\n' { iniBuilder->buildVersion ($2); }
 | '\n'
 | error { yyerror ("unrecognized line in setup.ini headers (do you have the latest setup?)"); } '\n'
 ;

packages
 : package packages
 | /* empty */
 ;

package
 : '@' STRING '\n'		{ iniBuilder->buildPackage ($2);}
   lines
 ;

lines
 : lines '\n' simple_line
 | simple_line
 ;

simple_line
 : PACKAGEVERSION STRING	{ iniBuilder->buildPackageVersion ($2); }
 | SDESC STRING			{ iniBuilder->buildPackageSDesc($2); }
 | LDESC STRING			{ iniBuilder->buildPackageLDesc($2); }
 | CATEGORY categories
 | REQUIRES requires
 | INSTALL STRING STRING MD5    { iniBuilder->buildPackageInstall ($2, $3, $4); }
 | INSTALL STRING STRING	{ iniBuilder->buildPackageInstall ($2, $3); }
 | SOURCE STRING STRING MD5	{ iniBuilder->buildPackageSource ($2, $3, $4); }
 | SOURCE STRING STRING		{ iniBuilder->buildPackageSource ($2, $3); }
 | T_PREV			{ iniBuilder->buildPackageTrust (TRUST_PREV); }
 | T_CURR			{ iniBuilder->buildPackageTrust (TRUST_CURR); }
 | T_TEST			{ iniBuilder->buildPackageTrust (TRUST_TEST); }
 | T_UNKNOWN			{ iniBuilder->buildPackageTrust (TRUST_UNKNOWN); }
 | /* empty */
 | error { yyerror (String("unrecognized line ") + yylineno + " (do you have the latest setup?)");
	}
 ;

requires
 : STRING			{ iniBuilder->buildPackageRequirement($1); } requires
 | STRING			{ iniBuilder->buildPackageRequirement($1); }
 ;

categories
 : STRING			{ iniBuilder->buildPackageCategory ($1); } categories
 | STRING			{ iniBuilder->buildPackageCategory ($1); }
 ;

%%
