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

LogStream::LogStream(LogFile *parent, log_level level)
  : std::ostream(nullptr), parent_(parent), level_(level), buffer_(nullptr)
{
  rdbuf(std::cerr.rdbuf());
}

LogStream::~LogStream()
{
}

std::unique_ptr<LogStream> LogFile::getStream(enum log_level level)
{
  return std::unique_ptr<LogStream>(new LogStream(this, level));
}

std::ostream& endLog(std::ostream& outs)
{
  outs << std::endl;
  return outs;
}
