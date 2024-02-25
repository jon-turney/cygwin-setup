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

#include "Feedback.h"

class CliFeedback : public Feedback
{
  // ini parse
public:
  virtual void parse_init ();
  virtual void parse_finish ();
  virtual void progress (unsigned long const pos, unsigned long const max);
  virtual void iniName (const std::string& name);
  virtual void babble (const std::string& message) const;
  virtual void warning (const std::string& message) const;
  virtual void show_errors () const;
  virtual void note_error(int lineno, const std::string &s);
  virtual bool has_errors () const;
private:
  int error_count = 0;

  // URL fetch
public:
  void fetch_progress_disable (bool);
  void fetch_init (const std::string &url, int length);
  void fetch_set_length(int length);
  void fetch_set_total_length(long long int total_length);
  void fetch_progress (int bytes);
  void fetch_total_progress ();
  void fetch_finish (int total_bytes);
  void fetch_fatal (const char *filename, const char *err);

private:
  int max_bytes;
  long long int total_download_bytes = 0; // meaning ???
  long long int total_download_bytes_sofar = 0;

  unsigned int last_tics;
  unsigned int start_tics;

  // hash checking
public:
  void hash_init (const char *hashalg, const std::string &url);
  void hash_progress (int bytes, int total_bytes);

  // owner
public:
  HWND owner () { return NULL; }

};
