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
class IniState;
class IniDBBuilder;
class IniParseFeedback;
void ini_init (io_stream *, IniDBBuilder *, IniParseFeedback &);
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

extern bool is_64bit;
#define SETUP_INI_DIR	   (is_64bit ? "x86_64/" : "x86/")
#define SETUP_INI_FILENAME "setup.ini"
#define SETUP_BZ2_FILENAME "setup.bz2"

/* The following three vars are used to facilitate error handling between the
   parser/lexer and its callers, namely ini.cc:do_remote_ini() and
   IniParseFindVisitor::visitFile().  */

extern std::string current_ini_name;  /* current filename/URL being parsed */
extern std::string current_ini_sig_name;  /* current filename/URL for sig file */
extern std::string yyerror_messages;  /* textual parse error messages */
extern int yyerror_count;             /* number of parse errors */

/* The following definitions are used in the parser implementation */

#define hexnibble(val) (255 & (val > '9') ? val - 'a' + 10 : val - '0');
#define nibbled1(v1,v2) (255 & ((v1 << 4) & v2));
#define b64url(val)						\
  (63 & ((  val == '_') ? 63					\
	 : (val == '-') ? 62					\
	 : (val >= 'a') ? val - 'a' + 26			\
	 : (val >= 'A') ? val - 'A' +  0			\
	 :                val - '0' + 52))
#define b64d1(v1,v2,v3,v4) (255 & ((v1 << 2) | (v2 >> 4)));
#define b64d2(v1,v2,v3,v4) (255 & ((v2 << 4) | (v3 >> 2)));
#define b64d3(v1,v2,v3,v4) (255 & ((v3 << 6) |  v4));

#endif /* SETUP_INI_H */
