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

#include "win32.h"
#define PSAPI_VERSION 1
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>

#include "processlist.h"
#include <String++.h>
#include "LogSingleton.h"
#include "script.h"
#include "mount.h"
#include "filemanip.h"

// ---------------------------------------------------------------------------
// implements class Process
//
// access to a Windows process
// ---------------------------------------------------------------------------

Process::Process (DWORD pid) : processID (pid)
{
}

std::string
Process::getName (void)
{
  HANDLE hProcess = OpenProcess (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
  char modName[CYG_PATH_MAX];
  GetModuleFileNameExA (hProcess, NULL, modName, sizeof (modName));
  CloseHandle (hProcess);

  std::string s = modName;
  s += " (pid " + stringify (processID) + ")";
  return s;
}

DWORD
Process::getProcessID (void)
{
  return processID;
}

void
Process::kill (int force)
{
  if (force >= 2)
    {
      // use TerminateProcess() to request force-killing of the process
      HANDLE hProcess = OpenProcess (PROCESS_TERMINATE, FALSE, processID);

      if (hProcess)
        {
          if (TerminateProcess(hProcess, (UINT)-1))
            {
              Log (LOG_BABBLE) << "TerminateProcess succeeded on pid " << processID << endLog;
            }
          else
            {
              Log (LOG_BABBLE) << "TerminateProcess failed " << GetLastError () << " for pid " << processID << endLog;
            }
          CloseHandle (hProcess);
        }

      return;
    }

  std::string signame;

  switch (force)
    {
      case 0:
        signame = "-TERM";
        break;

      default:
      case 1:
        signame = "-KILL";
        break;
     }

  std::string kill_cmd = backslash (cygpath ("/bin/kill.exe")) + " " + signame + " " + stringify (processID);
  ::run (kill_cmd.c_str ());
}

//
// test if a module is loaded into a process
//
bool
Process::isModuleLoadedInProcess (const WCHAR *moduleName)
{
  BOOL match = FALSE;

  // Get process handle
  HANDLE hProcess = OpenProcess (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

  if (NULL == hProcess)
    return FALSE;

  static unsigned int bytesAllocated = 0;
  static HMODULE *hMods = 0;

  // initial allocation
  if (bytesAllocated == 0)
    {
      bytesAllocated = sizeof (HMODULE)*1024;
      hMods = (HMODULE *)malloc (bytesAllocated);
    }

  while (1)
    {
      DWORD cbNeeded;

      // Get a list of all the modules in this process.
      if (!EnumProcessModules (hProcess, hMods, bytesAllocated, &cbNeeded))
        {
          // Don't log ERROR_PARTIAL_COPY as expected for System process and those of different bitness
          if (GetLastError () != ERROR_PARTIAL_COPY)
            {
              Log (LOG_BABBLE) << "EnumProcessModules failed " << GetLastError () << " for pid " << processID << endLog;
            }

          cbNeeded = 0;
        }

      // If we didn't get all modules, retry with a larger array
      if (cbNeeded > bytesAllocated)
        {
          bytesAllocated = cbNeeded;
          hMods = (HMODULE *)realloc (hMods, bytesAllocated);
          continue;
        }

      // Search module list for the module we are looking for
      for (unsigned i = 0; i < (cbNeeded / sizeof (HMODULE)); i++ )
        {
          WCHAR szModName[CYG_PATH_MAX];

          // Get the full path to the module's file.
          if (GetModuleFileNameExW (hProcess, hMods[i], szModName, sizeof (szModName)/sizeof (WCHAR)))
            {
              WCHAR canonicalModName[CYG_PATH_MAX];

              // Canonicalise returned module name to long UNC form
              if (wcscmp (szModName, L"\\\\?\\") != 0)
                {
                  wcscpy (canonicalModName, L"\\\\?\\");
                  wcscat (canonicalModName, szModName);
                }
              else
                {
                  wcscpy (canonicalModName, szModName);
                }

              // Does it match the name ?
              if (wcscmp (moduleName, canonicalModName) == 0)
                {
                  match = TRUE;
                  break;
                }
            }
        }

      break;
    }

    // Release the process handle
    CloseHandle (hProcess);

    return match;
}

//
// get a list of currently running processes
//
ProcessList
Process::snapshot (void)
{
  static DWORD *pProcessIDs = 0;
  static unsigned int bytesAllocated = 0;
  DWORD bytesReturned;

  // initial allocation
  if (bytesAllocated == 0)
    {
      bytesAllocated = sizeof (DWORD);
      pProcessIDs = (DWORD *)malloc (bytesAllocated);
    }

  // fetch a snapshot of process list
  while (1)
    {
      if (!EnumProcesses (pProcessIDs, bytesAllocated, &bytesReturned))
        {
          Log (LOG_BABBLE) << "EnumProcesses failed " << GetLastError () << endLog;
          bytesReturned = 0;
        }

      // If we didn't get all processes, retry with a larger array
      if (bytesReturned == bytesAllocated)
        {
          bytesAllocated = bytesAllocated*2;
          pProcessIDs = (DWORD *)realloc (pProcessIDs, bytesAllocated);
          continue;
        }

      break;
    }

  // convert to ProcessList vector
  unsigned int nProcesses = bytesReturned/sizeof (DWORD);
  ProcessList v(nProcesses, 0);
  for (unsigned int i = 0; i < nProcesses; i++)
    {
      v[i] = pProcessIDs[i];
    }

  return v;
}

//
// list processes which have a given executable module loaded
//
ProcessList
Process::listProcessesWithModuleLoaded (const WCHAR *moduleName)
{
  ProcessList v;
  ProcessList pl = snapshot ();

  for (ProcessList::iterator i = pl.begin (); i != pl.end (); i++)
    {
      if (i->isModuleLoadedInProcess (moduleName))
        {
          v.push_back (*i);
        }
    }

  return v;
}
