/*
 * Copyright (c) 2002, Robert Collins..
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <rbtcollins@hotmail.com>
 *
 */

#ifndef SETUP_LOGFILE_H
#define SETUP_LOGFILE_H

#include <iostream>
#include <sstream>
#include <memory>
#include <mutex>

enum log_level {
  LOG_PLAIN = 2,
  LOG_BABBLE = 1,
  LOG_TIMESTAMP = 2
};

// Forward declaration
class LogFile;

// Custom ostream subclass that carries context for thread-safe logging.
// Each getStream() call returns a fresh LogStream instance with its own buffer.
class LogStream : public std::ostream
{
public:
  LogStream(LogFile *parent, log_level level);
  ~LogStream();

  LogFile *getParent() const { return parent_; }
  log_level getLevel() const { return level_; }
  std::stringbuf *getBuffer() const { return buffer_; }

private:
  LogFile *parent_;
  log_level level_;
  std::stringbuf *buffer_;
};

// Logging class.
class LogFile : public std::ostream {
public:
  LogFile();
  ~LogFile();

  void clearFiles(); // delete all target filenames
  void setFile (int minlevel, const std::string& path, bool append);
  std::string getFileName (int level) const;

  void saveAll ();

  // get a specific verbosity stream.
  std::unique_ptr<LogStream> getStream(enum log_level level);

  friend std::ostream& endLog(std::ostream& outs);

protected:
  LogFile (LogFile const &); // no copy constructor
  LogFile &operator = (LogFile const&); // no assignment operator


  void endEntry(LogStream &stream); // the current in-progress entry is complete.

private:
  void log_save (int babble, const std::string& filename, bool append);
  std::mutex log_mutex;
};

// End of a Log comment
extern std::ostream& endLog(std::ostream& outs);

// Log adapators for printf-style output
void LogBabblePrintf(const char *fmt, ...);
void LogPlainPrintf(const char *fmt, ...);

// Global instance
LogFile &getGlobalLogger();

#define Log(X) (*(getGlobalLogger ().getStream (X)))

#define Logger() (getGlobalLogger ())

#endif /* SETUP_LOGFILE_H */
