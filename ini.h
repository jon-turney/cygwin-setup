/*
 * Copyright (c) 2000,2007 Red Hat, Inc.
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

#ifndef SETUP_INI_H
#define SETUP_INI_H

class io_stream;
#include <string>
#include <vector>

typedef std::vector <std::string> IniList;
extern IniList found_ini_list, setup_ext_list;
const std::string setup_exts[] = { "xz", "bz2", "ini" };
extern bool is_64bit;
extern bool is_new_install;
extern std::string SetupArch;
extern std::string SetupIniDir;
extern std::string SetupBaseName;

class IniState;
class IniDBBuilderPackage;
class IniParseFeedback;
void ini_init (io_stream *, IniDBBuilderPackage *, IniParseFeedback &);
#define YYSTYPE char *

/* When setup.ini is parsed, the information is stored according to
   the declarations here.  ini.cc (via inilex and iniparse)
   initializes these structures.  choose.cc sets the action and trust
   fields.  download.cc downloads any needed files for selected
   packages (the chosen "install" field).  install.cc installs
   selected packages. */

typedef enum
{
  EXCLUDE_NONE = 0,
  EXCLUDE_BY_SETUP,
  EXCLUDE_NOT_FOUND
} excludes;

/* The following three vars are used to facilitate error handling between the
   parser/lexer and its callers, namely ini.cc:do_remote_ini() and
   IniParseFindVisitor::visitFile().  */

extern std::string current_ini_name;  /* current filename/URL being parsed */
extern std::string current_ini_sig_name;  /* current filename/URL for sig file */
extern std::string yyerror_messages;  /* textual parse error messages */
extern int yyerror_count;             /* number of parse errors */

/* The following definitions are used in the parser implementation */

#define hexnibble(val)  ('\xff' & (val > '9') ? val - 'a' + 10 : val - '0')
#define nibbled1(v1,v2) ('\xff' & ((v1 << 4) | v2))
#define b64url(val)						\
  ('\x3f' & ((  val == '_') ? '\x3f'				\
	     : (val == '-') ? '\x3e'				\
	     : (val >= 'a') ? val - 'a' + '\x1a'		\
	     : (val >= 'A') ? val - 'A' + '\x00'		\
	     :                val - '0' + '\x34'))
#define b64d1(v1,v2,v3,v4) ('\xff' & ((v1 << 2) | (v2 >> 4)))
#define b64d2(v1,v2,v3,v4) ('\xff' & ((v2 << 4) | (v3 >> 2)))
#define b64d3(v1,v2,v3,v4) ('\xff' & ((v3 << 6) |  v4))

#endif /* SETUP_INI_H */
