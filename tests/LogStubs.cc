/*
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#include "LogFile.h"

// Global logger instance
static LogFile global_logger;

LogFile &getGlobalLogger()
{
  return global_logger;
}

// Stub implementation
LogFile::LogFile()
{
}

LogFile::~LogFile()
{
}

std::ostream & LogFile::getStream(enum log_level level)
{
  return std::cerr;
}

std::ostream& endLog(std::ostream& outs)
{
  outs << std::endl;
  return outs;
}
