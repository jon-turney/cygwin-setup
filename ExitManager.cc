/*
 * Copyright (c) 2026, Cygwin Contributors.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Jon Turney <jon.turney@dronecode.org.uk>
 *
 */

#include "win32.h"
#include "LogFile.h"
#include "String++.h"
#include "filemanip.h"
#include "ExitManager.h"

#include <stdlib.h>
#include <string>

// This is fucking terrible. But LogSingleton is worse. So for the moment,
// separate out the concerns of managing our exit status, which have nothing to
// do with logging, so we can address the issues there...

int ExitManager::exit_msg = 0;
std::vector <void (*)(void)> ExitManager::exit_fns;

void
ExitManager::atexit(void (*func)(void))
{
  exit_fns.push_back(func);
}

void
ExitManager::exit (int exit_code, bool show_end_install_msg)
{
  /* Execute any functions we want to run at exit (we don't use stdlib atexit()
     because we want to allow them to potentially write to the log) */
  for (auto i = exit_fns.rbegin(); i != exit_fns.rend(); ++i)
      (*i)();

  static int been_here = 0;
  if (been_here)
    ::exit (exit_code);
  been_here = 1;

  if (exit_msg)
    {
      std::wstring fmt = LoadStringWEx(exit_msg, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
      std::wstring buf = format(fmt, backslash(Logger().getFileName(LOG_BABBLE)).c_str());
      Log (LOG_PLAIN) << "note: " << wstring_to_string(buf) << endLog;
    }

  /* Skip this final message when we're just printing the help/version output,
     and when we're self-elevating. */
  if (show_end_install_msg)
    Log (LOG_PLAIN) << "Ending cygwin install" << endLog;

  /* Flush all log entries to files */
  Logger().saveAll();

  ::exit (exit_code);
}
