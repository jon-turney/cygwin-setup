/*
 * Copyright (c) 2020 Jon Turney
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#include "CliParseFeedback.h"
#include "LogSingleton.h"
#include <sstream>
#include <iostream>

void CliParseFeedback::progress (unsigned long const pos, unsigned long const max)
{
  std::cout << pos << "/" << max << std::endl;
}

void CliParseFeedback::iniName (const std::string& name)
{
}

void CliParseFeedback::babble (const std::string& message) const
{
  Log (LOG_BABBLE) << message << endLog;
}

void CliParseFeedback::warning (const std::string& message) const
{
  std::cout << "Warning: " << message << std::endl;
}

void CliParseFeedback::show_errors () const
{
}

void CliParseFeedback::note_error(int lineno, const std::string &s)
{
  std::ostringstream buf;
  buf << "line " << lineno << ": ";
  buf << s << std::endl;
  std::cout << buf.str();
  error_count++;
}

bool CliParseFeedback::has_errors () const
{
  return (error_count > 0);
}
