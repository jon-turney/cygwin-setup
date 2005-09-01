/*
 * Copyright (c) 2001, Jan Nieuwenhuizen.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Jan Nieuwenhuizen <janneke@gnu.org>
 *
 */
#ifndef SETUP_SCRIPT_H
#define SETUP_SCRIPT_H

#include "String++.h"
   
/* Initialisation stuff for run_script: sh, cmd, CYGWINROOT and PATH */
void init_run_script ();

/* Run the script named dir/fname.ext
   Returns the script exit status or negative error if any.  */
int try_run_script (String const &dir, String const &fname, String const &ext);

class Script {
public:
  static bool isAScript (String const &file);
  Script (String const &fileName);
  String baseName() const;
  String fullName() const;
/* Run the script.  If its suffix is .sh, and we have a Bourne shell, execute
   it using sh.  Otherwise, if the suffix is .bat, execute using cmd.exe (NT)
   or command.com (9x).  Returns the exit status of the process, or 
   negative error if any.  */
  int run() const;
private:
  String scriptName;
  static char const ETCPostinstall[];
  char const * extension() const;
};

#endif /* SETUP_SCRIPT_H */
