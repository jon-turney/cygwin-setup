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

#include <vector>

class ExitManager
{
public:
  static void setExitMsg (int msg) { exit_msg = msg; }
  static int getExitMsg () { return exit_msg; }

  static void exit (int exit_code, bool show_end_install_msg = true)
          __attribute__ ((noreturn));
  static void atexit( void (*func)(void));

private:
  static int exit_msg;
  static std::vector <void (*)(void)> exit_fns;
};
