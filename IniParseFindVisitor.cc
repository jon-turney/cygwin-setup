/*
 * Copyright (c) 2002 Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <robertc@hotmail.com>
 *
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "IniParseFindVisitor.h"
#include "IniParseFeedback.h"
#include "IniDBBuilder.h"
#include "io_stream.h"
#include "rfc1738.h"
#include "ini.h"
#include <stdexcept>

using namespace std;

extern int yyparse ();

IniParseFindVisitor::IniParseFindVisitor(IniDBBuilder &aBuilder, String const &localroot, IniParseFeedback &feedback) : _Builder (aBuilder), _feedback (feedback), baseLength (localroot.size()), local_ini(0),
error_buf(0), error_count (0),
setup_timestamp (0), setup_version() {}
IniParseFindVisitor::~IniParseFindVisitor(){}

/* look for potential packages we can add to the in-memory package
 * database
 */
void
IniParseFindVisitor::visitFile(String const &basePath, const WIN32_FIND_DATA *theFile)
{
  //TODO: Test for case sensitivity issues
  if (String("setup.ini").casecompare(theFile->cFileName))
    return;

  String path = basePath + theFile->cFileName;
  
  io_stream *ini_file = io_stream::open (String ("file://") + path, "rb");

  if (!ini_file)
    // We don't throw an exception, because while this is fatal to parsing, it
    // isn't to the visitation.
    {
      // This should never happen
      // If we want to handle it happening, use the log strategy call
      throw new runtime_error ("IniParseFindVisitor: failed to open ini file, which should never happen");
      return;
    }
  
  _feedback.babble (String ("Found ini file - ") + path);
  _feedback.iniName (path);
  
  /* Copy leading part of path to temporary buffer and unescape it */
  
  String prefix (&basePath.cstr_oneuse()[baseLength + 1]);
  String mirror;
  if (prefix.size())
    mirror = rfc1738_unescape_part (prefix.substr(0,prefix.size() - 1));
  else
    mirror = ".";
  _Builder.parse_mirror = mirror;
  ini_init (ini_file, &_Builder, _feedback);
  
  /*yydebug = 1; */

  if (yyparse () || error_count > 0)
    _feedback.error(error_buf);
  else
    local_ini++;
  
  if (error_buf)
    *error_buf = '\0';
  error_count = 0;
  
  if (_Builder.timestamp > setup_timestamp)
    {
      setup_timestamp = _Builder.timestamp;
      setup_version = _Builder.version;
    }
}

int
IniParseFindVisitor::iniCount() const
{
  return local_ini;
}

unsigned int 
IniParseFindVisitor::timeStamp () const
{
  return setup_timestamp;
}

String
IniParseFindVisitor::version() const
{
  return setup_version;
}
