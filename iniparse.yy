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

#include <string>
#include "win32.h"
#include "ini.h"
#include "iniparse.hh"
#include "PackageTrust.h"

extern int yyerror (const std::string& s);
int yylex ();

#include "IniDBBuilderPackage.h"

#define YYERROR_VERBOSE 1
#define YYINITDEPTH 1000
/*#define YYDEBUG 1*/

IniDBBuilderPackage *iniBuilder;
extern int yylineno;
%}

%token STRING
%token SETUP_TIMESTAMP
%token SETUP_VERSION
%token SETUP_MINIMUM_VERSION
%token PACKAGEVERSION
%token INSTALL
%token SOURCE
%token SDESC
%token LDESC
%token REPLACE_VERSIONS
%token CATEGORY
%token DEPENDS
%token REQUIRES
%token PROVIDES
%token CONFLICTS
%token T_PREV T_CURR T_TEST T_OTHER
%token MD5 SHA512
%token SOURCEPACKAGE
%token PACKAGENAME
%token COMMA NL AT
%token OPENBRACE CLOSEBRACE EQUAL GT LT GTEQUAL LTEQUAL 
%token BUILDDEPENDS
%token OBSOLETES
%token MESSAGE
%token ARCH RELEASE

%%

whole_file
 : setup_headers packageseparator packages
 ;

setup_headers: /* empty */
 | setup_headers header
 ;

header /* non-empty */
 : SETUP_TIMESTAMP STRING	{ iniBuilder->buildTimestamp ($2); } NL
 | SETUP_VERSION STRING		{ iniBuilder->buildVersion ($2); } NL
 | RELEASE STRING		{ iniBuilder->set_release ($2); } NL
 | ARCH STRING			{ iniBuilder->set_arch ($2); } NL
 | SETUP_MINIMUM_VERSION STRING { std::string e = iniBuilder->buildMinimumVersion ($2); if (!e.empty()) { yyerror(e); } } NL
 ;

packages: /* empty */
 | packages package packageseparator
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
 | T_OTHER NL			{ iniBuilder->buildPackageTrust (TRUST_OTHER); }
 | SOURCEPACKAGE source NL
 | CATEGORY categories NL
 | INSTALL STRING STRING MD5 NL { iniBuilder->buildPackageInstall ($2, $3, $4, hashType::md5); }
 | INSTALL STRING STRING SHA512 NL { iniBuilder->buildPackageInstall ($2, $3, $4, hashType::sha512); }
 | SOURCE STRING STRING MD5 NL {iniBuilder->buildPackageSource ($2, $3, $4, hashType::md5); }
 | SOURCE STRING STRING SHA512 NL {iniBuilder->buildPackageSource ($2, $3, $4, hashType::sha512); }
 | DEPENDS { iniBuilder->buildBeginDepends(); } versionedpackagelist NL
 | REQUIRES { iniBuilder->buildBeginDepends(); } versionedpackagelistsp NL
 | BUILDDEPENDS { iniBuilder->buildBeginBuildDepends(); } versionedpackagelist NL
 | OBSOLETES { iniBuilder->buildBeginObsoletes(); } versionedpackagelist NL
 | PROVIDES { iniBuilder->buildBeginProvides(); } versionedpackagelist NL
 | CONFLICTS { iniBuilder->buildBeginConflicts(); } versionedpackagelist NL
 | REPLACE_VERSIONS versionlist NL

 | MESSAGE STRING STRING NL	{ iniBuilder->buildMessage ($2, $3); }
 | error NL			{ yyerror (std::string("unrecognized line ")
					  + stringify(yylineno)
					  + " (do you have the latest setup?)");
				}
 ;

categories: /* empty */
 | categories STRING		{ iniBuilder->buildPackageCategory ($2); }
 ;

source /* non-empty */
 : STRING { iniBuilder->buildSourceName ($1); } versioninfo
 ;

versioninfo: /* empty */
 | OPENBRACE STRING CLOSEBRACE { iniBuilder->buildSourceNameVersion ($2); }
 ;

versionedpackagelist: /* empty */
 | versionedpackageentry
 | versionedpackagelist listseparator versionedpackageentry
 ;

versionedpackagelistsp /* non-empty */
 : versionedpackageentry
 | versionedpackagelistsp versionedpackageentry
 ;

listseparator /* non-empty */
 : COMMA
 | COMMA NL
 ;

versionedpackageentry /* non-empty */
 : STRING { iniBuilder->buildPackageListNode($1); } versioncriteria
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

versionlist: /* empty */
 | versionlist STRING { iniBuilder->buildPackageReplaceVersionsList ($2); }
 ;

%%
