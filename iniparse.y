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
%token CATEGORY DEPENDS REQUIRES
%token APATH PPATH INCLUDE_SETUP EXCLUDE_PACKAGE DOWNLOAD_URL
%token T_PREV T_CURR T_TEST T_UNKNOWN
%token MD5 INSTALLEDSIZE MAINTAINER PRIORITY
%token DESCTAG DESCRIPTION FILESIZE ARCHITECTURE SOURCEPACKAGE MD5LINE 
%token RECOMMENDS PREDEPENDS
%token SUGGESTS CONFLICTS REPLACES PROVIDES PACKAGENAME STRTOEOL PARAGRAPH LEX_EOF
%token EMAIL COMMA OR NL AT
%token OPENBRACE CLOSEBRACE EQUAL GT LT GTEQUAL LTEQUAL 

%%

whole_file
 : setup_headers packages
 ;

setup_headers: /* empty */
 | setup_headers header
 ;
 
header /* non-empty */
 : SETUP_TIMESTAMP STRING { iniBuilder->buildTimestamp ($2); } NL
 | SETUP_VERSION STRING { iniBuilder->buildVersion ($2); } NL
 ;

packages: /* empty */
 | packages packageseparator package
 ;

packageseparator: /* empty */
 | packageseparator NL
 ;
 
package /* non-empty */
 : packagename NL packagedata 
 ;

packagename /* non-empty */
 : AT STRING		{ iniBuilder->buildPackage ($2); }
 | PACKAGENAME STRING	{ iniBuilder->buildPackage ($2); }
 ;

packagedata: /* empty */
 | packagedata singleitem
 ;

singleitem /* non-empty */
 : PACKAGEVERSION STRING NL	{ iniBuilder->buildPackageVersion ($2); }
 | SDESC STRING NL		{ iniBuilder->buildPackageSDesc($2); }
 | LDESC STRING NL		{ iniBuilder->buildPackageLDesc($2); }
 | T_PREV NL 			{ iniBuilder->buildPackageTrust (TRUST_PREV); }
 | T_CURR NL			{ iniBuilder->buildPackageTrust (TRUST_CURR); }
 | T_TEST NL			{ iniBuilder->buildPackageTrust (TRUST_TEST); }
 | T_UNKNOWN NL		      { iniBuilder->buildPackageTrust (TRUST_UNKNOWN);}
 | PRIORITY STRING NL		{ iniBuilder->buildPriority ($2); }
 | INSTALLEDSIZE STRING NL	{ iniBuilder->buildInstalledSize ($2); }
 | MAINTAINER STRING NL		{ iniBuilder->buildMaintainer ($2); }
 | ARCHITECTURE STRING NL 	{ iniBuilder->buildArchitecture ($2); }
 | FILESIZE STRING NL		{ iniBuilder->buildInstallSize($2); }
 | MD5LINE MD5 NL	{ iniBuilder->buildInstallMD5 ((unsigned char *)$2); }
 | SOURCEPACKAGE source NL
 | CATEGORY categories NL
 | INSTALL STRING { iniBuilder->buildPackageInstall ($2); } installmeta NL
 | SOURCE STRING STRING sourceMD5 NL {iniBuilder->buildPackageSource ($2, $3);}
 | PROVIDES 		{ iniBuilder->buildBeginProvides(); } packagelist NL
 | CONFLICTS	{ iniBuilder->buildBeginConflicts(); } versionedpackagelist NL
 | DEPENDS { iniBuilder->buildBeginDepends(); } versionedpackagelist NL
 | REQUIRES { iniBuilder->buildBeginDepends(); }versionedpackagelistsp NL
 | PREDEPENDS { iniBuilder->buildBeginPreDepends(); } versionedpackagelist NL
 | RECOMMENDS { iniBuilder->buildBeginRecommends(); }   versionedpackagelist NL
 | SUGGESTS { iniBuilder->buildBeginSuggests(); } versionedpackagelist NL
 | REPLACES { iniBuilder->buildBeginReplaces(); }       versionedpackagelist NL
 | DESCTAG mlinedesc
 | LEX_EOF			{ YYACCEPT; }
 | error 			{ yyerror (String("unrecognized line ") 
					  + yylineno 
					  + " (do you have the latest setup?)");
				}
 ;

categories: /* empty */
 | categories STRING		{ iniBuilder->buildPackageCategory ($2); }
 ;

installmeta: /* empty */
 | STRING installMD5		{ iniBuilder->buildInstallSize($1); }
 ;

installMD5: /* empty */
 | MD5 			{ iniBuilder->buildInstallMD5 ((unsigned char *)$1);}
 ;

sourceMD5: /* empty */
 | MD5 			{ iniBuilder->buildSourceMD5 ((unsigned char *)$1); }
 ;

source /* non-empty */
 : STRING { iniBuilder->buildSourceName ($1); } versioninfo
 ;

versioninfo: /* empty */
 | OPENBRACE STRING CLOSEBRACE { iniBuilder->buildSourceNameVersion ($2); }
 ;

mlinedesc: /* empty */
 | mlinedesc STRTOEOL NL	{ iniBuilder->buildDescription ($2); }
 | mlinedesc STRTOEOL PARAGRAPH { iniBuilder->buildDescription ($2); }
 ;

packagelist /* non-empty */
 : packagelist COMMA { iniBuilder->buildPackageListAndNode(); } packageentry
 | { iniBuilder->buildPackageListAndNode(); } packageentry
 ;

packageentry /* empty not allowed */
 : STRING 		{ iniBuilder->buildPackageListOrNode($1); }
 | packageentry OR STRING { iniBuilder->buildPackageListOrNode($3); }
 ;

versionedpackagelist /* non-empty */
 : { iniBuilder->buildPackageListAndNode(); } versionedpackageentry
 | versionedpackagelist listseparator { iniBuilder->buildPackageListAndNode(); } versionedpackageentry
 ;

versionedpackagelistsp /* non-empty */
 : { iniBuilder->buildPackageListAndNode(); } versionedpackageentry
 | versionedpackagelistsp { iniBuilder->buildPackageListAndNode(); } versionedpackageentry
 ;


listseparator /* non-empty */
 : COMMA
 | COMMA NL
 ;
 
versionedpackageentry /* empty not allowed */
 : STRING { iniBuilder->buildPackageListOrNode($1); } versioncriteria
 | versionedpackageentry OR STRING { iniBuilder->buildPackageListOrNode($3); } versioncriteria
 ;

versioncriteria: /* empty */
 | OPENBRACE operator STRING CLOSEBRACE { iniBuilder->buildPackageListOperatorVersion ($3); }
 ;
 
operator /* non-empty */
 : EQUAL { iniBuilder->buildPackageListOperator (PackageSpecification::Equals); }
 | LT { iniBuilder->buildPackageListOperator (PackageSpecification::LessThan); }
 | GT { iniBuilder->buildPackageListOperator (PackageSpecification::MoreThan); }
 | LTEQUAL { iniBuilder->buildPackageListOperator (PackageSpecification::LessThanEquals); }
 | GTEQUAL { iniBuilder->buildPackageListOperator (PackageSpecification::MoreThanEquals); }
 ;
 
%%
