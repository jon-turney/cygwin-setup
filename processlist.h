/*
 * Copyright (c) 2013 Jon TURNEY
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

#include <windows.h>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// interface to class Process
//
// access to a Windows process
// ---------------------------------------------------------------------------

class Process;

// utility type ProcessList, a vector of Process
typedef std::vector<Process> ProcessList;

class Process
{
public:
  Process (DWORD pid);
  std::string getName (void);
  DWORD getProcessID (void);
  void kill (int force);
  bool isModuleLoadedInProcess (const WCHAR *moduleName);
  static ProcessList listProcessesWithModuleLoaded (const WCHAR *moduleName);
private:
  DWORD processID;
  static ProcessList snapshot (void);
};
