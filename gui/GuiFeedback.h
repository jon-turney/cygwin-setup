/*
 * Copyright (c) 2024 Jon Turney
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

class GuiFeedback : public Feedback
{
  // constructor
public:
  GuiFeedback(HWND hwnd) : owner_window(hwnd) { };

  // ini parse
public:
  void parse_init ();
  void parse_finish ();
  void progress (unsigned long const, unsigned long const);
  void iniName (const std::string& );
  void babble (const std::string& ) const;
  void warning (const std::string& ) const;
  void show_errors () const;
  void note_error(int lineno, const std::string &error);
  bool has_errors () const;

private:
  unsigned int lastpct;
  std::string filename;
  std::string yyerror_messages;
  int yyerror_count;

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
  int is_local_install = 0;
  int max_bytes = 0;
  long long int total_download_bytes = 0;
  long long int total_download_bytes_sofar = 0;
  DWORD start_tics;

public:
  // owner
  HWND owner () { return owner_window; }

private:
  HWND owner_window;
};
