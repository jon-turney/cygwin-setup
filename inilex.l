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

/* tokenize the setup.ini files.  We parse a string which we've
   previously downloaded.  The program must call ini_init() to specify
   that string. */

#include "win32.h"
#include <string.h>

#include "ini.h"
#include "iniparse.h"
#include "String++.h"

#define YY_INPUT(buf,result,max_size) { result = ini_getchar(buf, max_size); }

static int ini_getchar(char *buf, int max_size);
static void ignore_line (void);

%}

/*%option debug */
%option noyywrap
%option yylineno
%option never-interactive

STR	[a-zA-Z0-9_./+-]+

%%

[0123456789abcdef]{32,32}       {
    yylval = (char *) new unsigned char[16];
    for (int i=0; i< 16; ++i)
      ((unsigned char *) yylval) [i] = 0;
    for (int i=0; i< 32; ++i)
      {
	unsigned char val = (unsigned char) yytext[i];
	if (val > '9')
	  val = val - 'a' + 10;
	else
	  val = val - '0';
	((unsigned char *) yylval) [i / 2] += val << ((i % 2) ? 4 : 0);
      }
    return MD5;
}

\"[^"]*\"		{ yylval = new char [strlen (yytext+1) + 1];
			  strcpy (yylval, yytext+1);
			  yylval[strlen (yylval)-1] = 0;
			  return STRING; }

"setup-timestamp:"	return SETUP_TIMESTAMP;
"setup-version:"	return SETUP_VERSION;
"version:"		return PACKAGEVERSION;
"install:"		return INSTALL;
"source:"		return SOURCE;
"sdesc:"		return SDESC;
"ldesc:"		return LDESC;

"category:"		return CATEGORY;
"requires:"		return REQUIRES;

"apath:"		return APATH;
"ppath:"		return PPATH;

"include-setup:"	return INCLUDE_SETUP;

"download-url:"		return DOWNLOAD_URL;

^{STR}":"		ignore_line ();

"[curr]"		return T_CURR;
"[test]"		return T_TEST;
"[exp]"			return T_TEST;
"[prev]"		return T_PREV;
"["{STR}"]"		return T_UNKNOWN;

{STR}			{ yylval = new char [strlen(yytext) + 1];
  			  strcpy (yylval, yytext);
			  return STRING; }

[ \t\r]+		/* do nothing */;

"#".*\n			{ return '\n'; }

\n			{ return *yytext; }
.			{ return *yytext; }

%%

#include "io_stream.h"

static io_stream *input_stream = 0;
extern char *parse_mirror;

void
ini_init(io_stream *stream, String const &mirror)
{
  input_stream = stream;
  if (parse_mirror)
    delete[] parse_mirror;
  parse_mirror = mirror.cstr();
}

static int
ini_getchar(char *buf, int max_size)
{
  if (input_stream)
    {
      ssize_t len = input_stream->read (buf, max_size);
      if (len < 1)
        {
	  len = 0;
	  input_stream = 0;
	}
      return len;
    }
  return 0;
}

static void
ignore_line ()
{
  char c;
  while ((c = yyinput ()))
    {
      if (c == EOF)
	return;
      if (c == '\n')
	return;
    }
}
