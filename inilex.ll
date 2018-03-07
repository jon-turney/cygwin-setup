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
 * Maintained by Robert Collins <rbtcollins@hotmail.com>
 *
 */

/* tokenize the setup.ini files.  We parse a string which we've
   previously downloaded.  The program must call ini_init() to specify
   that string. */

#include "win32.h"
#include <string.h>

#include "ini.h"
#include "iniparse.hh"
#include "String++.h"
#include "IniParseFeedback.h"
#include "sha2.h"

#define YY_READ_BUF_SIZE 65536
#define YY_INPUT(buf,result,max_size) { result = ini_getchar(buf, max_size); }

static int ini_getchar(char *buf, int max_size);
static void ignore_line (void);

%}

/*%option debug */
%option nounput
%option noyywrap
%option yylineno
%option never-interactive

STR	[!a-zA-Z0-9_./:\+~-]+
HEX	[0-9a-f]
B64	[a-zA-Z0-9_-]

%%

{HEX}{32} {
    yylval = (char *) new unsigned char[16];
    memset (yylval, 0, 16);
    int i, j;
    unsigned char v1, v2;
    for (i = 0, j = 0; i < 32; i += 2, ++j)
      {
	v1 = hexnibble((unsigned char) yytext[i+0]);
	v2 = hexnibble((unsigned char) yytext[i+1]);
	((unsigned char *) yylval) [j] = nibbled1(v1, v2);
      }
    return MD5;
}

{HEX}{128} {
    yylval = (char *) new unsigned char[SHA512_DIGEST_LENGTH];
    memset (yylval, 0, SHA512_DIGEST_LENGTH);
    int i, j;
    unsigned char v1, v2;
    for (i = 0, j = 0; i < SHA512_BLOCK_LENGTH; i += 2, ++j)
      {
	v1 = hexnibble((unsigned char) yytext[i+0]);
	v2 = hexnibble((unsigned char) yytext[i+1]);
	((unsigned char *) yylval) [j] = nibbled1(v1, v2);
      }
    return SHA512;
}

{B64}{86} {
    /* base64url as defined in RFC4648 */
    yylval = (char *) new unsigned char[SHA512_DIGEST_LENGTH];
    memset (yylval, 0, SHA512_DIGEST_LENGTH);
    int i, j;
    unsigned char v1, v2, v3, v4;
    for (i = 0, j = 0; i < 4*(SHA512_DIGEST_LENGTH/3); i += 4, j += 3)
      {
	v1 = b64url(((unsigned char) yytext[i+0]));
	v2 = b64url(((unsigned char) yytext[i+1]));
	v3 = b64url(((unsigned char) yytext[i+2]));
	v4 = b64url(((unsigned char) yytext[i+3]));
	((unsigned char *) yylval) [j+0] = b64d1(v1, v2, v3, v4);
	((unsigned char *) yylval) [j+1] = b64d2(v1, v2, v3, v4);
	((unsigned char *) yylval) [j+2] = b64d3(v1, v2, v3, v4);
      }
    v1 = b64url((unsigned char) yytext[i+0]);
    v2 = b64url((unsigned char) yytext[i+1]);
    v3 = 0;
    v4 = 0;
    ((unsigned char *) yylval) [j+0] = b64d1(v1, v2, v3, v4);
    return SHA512;
}

\"[^"]*\"		{ yylval = new char [strlen (yytext+1) + 1];
			  strcpy (yylval, yytext+1);
			  yylval[strlen (yylval)-1] = 0;
			  return STRING; }

"setup-timestamp:"	return SETUP_TIMESTAMP;
"setup-version:"	return SETUP_VERSION;
"setup-minimum-version:"	return SETUP_MINIMUM_VERSION;
"arch:"			return ARCH;

"release:"		return RELEASE;
"Package:"		return PACKAGENAME;
[vV]"ersion:"		return PACKAGEVERSION;
"install:"|"Filename:"	return INSTALL;
"source:"		return SOURCE;
"sdesc:"		return SDESC;
"ldesc:"		return LDESC;
"message:"		return MESSAGE;
"Source:"		return SOURCEPACKAGE;
"Build-Depends:"	return BUILDDEPENDS;
"replace-versions:"	return REPLACE_VERSIONS;

"category:"|"Section:"	return CATEGORY;
"requires:"		return REQUIRES;
[dD]"epends:"		return DEPENDS;
[dD]"epends2:"		return DEPENDS;
[oO]"bsoletes:"	return OBSOLETES;
[pP]"rovides:"		return PROVIDES;
[cC]"onflicts:"	return CONFLICTS;

^{STR}":"		ignore_line ();

"[curr]"		return T_CURR;
"[test]"		return T_TEST;
"[exp]"			return T_TEST;
"[prev]"		return T_PREV;
"["{STR}"]"		return T_OTHER;

"("			return OPENBRACE;
")"			return CLOSEBRACE;
"<<"			return LT;
">>"			return GT;
">="			return GTEQUAL;
"<="			return LTEQUAL;
">"			return GT;
"<"			return LT;
"="                     return EQUAL;
\,			return COMMA;
"@"			return AT;

{STR}			{ yylval = new char [strlen(yytext) + 1];
  			  strcpy (yylval, yytext);
			  return STRING; }

[ \t\r]+		/* do nothing */;

^"#".*\n		/* do nothing */;

\n			{ return NL; }
.			{ return *yytext;}

%%

#include "io_stream.h"

static io_stream *input_stream = 0;
extern IniDBBuilderPackage *iniBuilder;
static IniParseFeedback *iniFeedback;
std::string current_ini_name, yyerror_messages;
int yyerror_count;

void
ini_init(io_stream *stream, IniDBBuilderPackage *aBuilder, IniParseFeedback &aFeedback)
{
  input_stream = stream;
  iniBuilder = aBuilder;
  iniFeedback = &aFeedback;
  YY_FLUSH_BUFFER;
  yylineno = 1;
  yyerror_count = 0;
  yyerror_messages.clear ();
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
      else
        iniFeedback->progress (input_stream->tell(), input_stream->get_size());
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

int
yyerror (const std::string& s)
{
  char tmp[16];
  sprintf (tmp, "%d", yylineno - (!!YY_AT_BOL ()));
  
  std::string e = current_ini_name + " line " + tmp + ": " + s;
  
  if (!yyerror_messages.empty ())
    yyerror_messages += "\n";

  yyerror_messages += e;
  // OutputDebugString (e.c_str ());
  yyerror_count++;
  /* TODO: is return 0 correct? */
  return 0;
}
