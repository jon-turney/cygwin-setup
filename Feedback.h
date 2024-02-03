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

#ifndef SETUP_FEEDBACK_H
#define SETUP_FEEDBACK_H

#include "win32.h"
#include <string>

/* Interface for feedback from ini parsing and URL fetching.
 *
 * Used to send feedback that users need but that should not interrupt
 * processing.
 *
 * Fatal errors are (may be) thrown as exceptions.
 */

class Feedback
{
public:
  // IniParsing
  virtual void parse_init () = 0;
  virtual void parse_finish () = 0;
  virtual void progress (unsigned long const, unsigned long const) = 0;
  virtual void iniName (const std::string& ) = 0;
  virtual void babble (const std::string& ) const = 0;
  virtual void warning (const std::string& ) const = 0;
  virtual void show_errors () const = 0;
  virtual void note_error(int lineno, const std::string &error) = 0;
  virtual bool has_errors () const = 0;

  // URL fetching
  virtual void fetch_progress_disable (bool) = 0;
  virtual void fetch_init (const std::string &url, int length) = 0;
  virtual void fetch_set_length(int length) = 0;
  virtual void fetch_set_total_length(long long int total_length) = 0;
  virtual void fetch_progress (int bytes) = 0;
  virtual void fetch_total_progress () = 0;
  virtual void fetch_finish (int total_bytes) = 0;
  virtual void fetch_fatal (const char *filename, const char *err) = 0;

  //
  virtual HWND owner () = 0;
};

#endif /* SETUP_FEEDBACK_H */
