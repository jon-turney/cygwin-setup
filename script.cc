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
 * Written by DJ Delorie <dj@cygnus.com>
 *            Jan Nieuwenhuizen <janneke@gnu.org>
 *
 */

/* The purpose of this file is to provide functions for the invocation
   of install scripts. */

#include "win32.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "LogSingleton.h"
#include "filemanip.h"
#include "mount.h"
#include "io_stream.h"
#include "script.h"
#include "mkdir.h"
#include "state.h"
#include "resource.h"
#if HAVE_ALLOCA_H
#include <alloca.h>
#else
#ifndef alloca
#define alloca __builtin_alloca
#endif
#endif

static std::string sh, dash;
static const char *cmd;

static void
sanitize_PATH ()
{
  char dummy;
  DWORD len = GetEnvironmentVariable ("PATH", &dummy, 0);
  char *path = (char *) alloca (len + 1);
  GetEnvironmentVariable ("PATH", path, len);
  std::string newpath = backslash (cygpath ("/bin") + ";"
				   + cygpath ("/usr/sbin") + ";"
				   + cygpath ("/sbin"));
  len = (UINT) GetSystemWindowsDirectory (&dummy, 0);
  char *system_root = (char *) alloca (len + 2);
  GetSystemWindowsDirectory (system_root, len--);
  if (system_root[len - 1] != '\\')
    {
      system_root[len] = '\\';
      system_root[++len] = '\0';
    }
  for (char *p = strtok (path, ";"); p; p = strtok (NULL, ";"))
    {
      size_t plen = strlen (p);
      size_t cmplen = plen == (len - 1) ? plen : len;
      if (strncasecmp (system_root, p, cmplen) == 0)
	{
	  newpath += ";";
	  newpath += p;
	}
    }
  SetEnvironmentVariable ("PATH", newpath.c_str());
}


void
init_run_script ()
{
  static bool initialized;
  if (initialized)
    return;

  initialized = true;

  char *env = GetEnvironmentStrings ();
  if (env)
    {
      for (char *p = env; *p; p = strchr (p, '\0') + 1)
	{
	  char *eq = strchr (p, '=');
	  *eq = '\0';
	  if (strcasecmp (p, "comspec") != 0
	      && strcasecmp (p, "path") != 0
	      && strncasecmp (p, "system", 7) != 0
	      && strncasecmp (p, "user", 4) != 0
	      && strcasecmp (p, "windir") != 0)
	    SetEnvironmentVariable (p, NULL);
	  p = eq + 1;
	}
      FreeEnvironmentStrings (env);
    }

  SetEnvironmentVariable ("CYGWINROOT", get_root_dir ().c_str());
  SetEnvironmentVariable ("CYGWINFORALL",
                          (root_scope == IDC_ROOT_SYSTEM) ? "-A" : NULL);
  sanitize_PATH ();
  SetEnvironmentVariable ("SHELL", "/bin/bash");
  SetEnvironmentVariable ("TEMP", backslash (cygpath ("/tmp")).c_str ());
  SetEnvironmentVariable ("TERM", "dumb");
  SetEnvironmentVariable ("TMP", "/tmp");

  sh   = backslash (cygpath ("/bin/bash.exe"));
  dash = backslash (cygpath ("/bin/dash.exe"));
  cmd  = "cmd.exe";
}

class OutputLog
{
public:
  OutputLog (const std::string& filename);
  ~OutputLog ();
  HANDLE handle () { return _handle; }
  BOOL isValid () { return _handle != INVALID_HANDLE_VALUE; }
  BOOL isEmpty () { return GetFileSize (_handle, NULL) == 0; }
  friend std::ostream &operator<< (std::ostream &, OutputLog &);
private:
  enum { BUFLEN = 1000 };
  HANDLE _handle;
  std::string _filename;
  void out_to(std::ostream &);
};

OutputLog::OutputLog (const std::string& filename)
  : _handle(INVALID_HANDLE_VALUE), _filename(filename)
{
  if (!_filename.size())
    return;

  SECURITY_ATTRIBUTES sa;
  memset (&sa, 0, sizeof (sa));
  sa.nLength = sizeof (sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  if (mkdir_p (0, backslash (cygpath (_filename)).c_str(), 0755))
    return;

  _handle = CreateFile (backslash (cygpath (_filename)).c_str(),
      GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
      &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
      NULL);

  if (_handle == INVALID_HANDLE_VALUE)
    {
      Log (LOG_PLAIN) << "error: Unable to redirect output to '" << _filename
		     << "'; using console" << endLog;
    }
}

OutputLog::~OutputLog ()
{
  if (_handle != INVALID_HANDLE_VALUE)
    CloseHandle (_handle);
  if (_filename.size() &&
      !DeleteFile(backslash (cygpath (_filename)).c_str()))
    {
      Log (LOG_PLAIN) << "error: Unable to remove temporary file '" << _filename
		     << "'" << endLog;
    }
}

std::ostream &
operator<< (std::ostream &out, OutputLog &log)
{
  log.out_to(out);
  return out;
}

void
OutputLog::out_to(std::ostream &out)
{
  char buf[BUFLEN];
  DWORD num;
  FlushFileBuffers (_handle);
  SetFilePointer(_handle, 0, NULL, FILE_BEGIN);

  while (ReadFile(_handle, buf, BUFLEN-1, &num, NULL) && num != 0)
    {
      buf[num] = '\0';
      out << buf;
    }

  SetFilePointer(_handle, 0, NULL, FILE_END);
}

int
run (const char *cmdline)
{

  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  DWORD flags = CREATE_NEW_CONSOLE;
  DWORD exitCode = 0;
  BOOL inheritHandles = FALSE;
  BOOL exitCodeValid = FALSE;

  Log (LOG_PLAIN) << "running: " << cmdline << endLog;

  char tmp_pat[] = "/var/log/setup.log.runXXXXXXX";
  OutputLog file_out = std::string (mktemp (tmp_pat));

  memset (&pi, 0, sizeof (pi));
  memset (&si, 0, sizeof (si));
  si.cb = sizeof (si);
  si.lpTitle = (char *) "Cygwin Setup Post-Install Script";
  si.dwFlags = STARTF_USEPOSITION;

  if (file_out.isValid ())
    {
      inheritHandles = TRUE;
      si.dwFlags |= STARTF_USESTDHANDLES;
      si.hStdInput = INVALID_HANDLE_VALUE;
      si.hStdOutput = file_out.handle ();
      si.hStdError = file_out.handle ();
      si.dwFlags |= STARTF_USESHOWWINDOW;
      si.wShowWindow = SW_HIDE;
      flags = CREATE_NO_WINDOW;
    }

  BOOL createSucceeded = CreateProcess (0, (char *)cmdline, 0, 0, inheritHandles,
					flags, 0, get_root_dir ().c_str(),
					&si, &pi);

  if (createSucceeded)
    {
      WaitForSingleObject (pi.hProcess, INFINITE);
      exitCodeValid = GetExitCodeProcess(pi.hProcess, &exitCode);
    }
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  if (!file_out.isEmpty ())
    Log (LOG_BABBLE) << file_out << endLog;

  if (exitCodeValid)
    return exitCode;
  return -GetLastError();
}

int
Script::run() const
{
  if ("done" == scriptExtension)
    return NO_ERROR;
  if (0 == scriptExtension.size())
    return -ERROR_INVALID_DATA;

  /* Bail here if the script file does not exist.  This can happen for
     example in the case of tetex-* where two or more packages contain a
     postinstall script by the same name.  When we are called the second
     time the file has already been renamed to .done, and if we don't
     return here we end up erroneously deleting this .done file.  */
  std::string windowsName = backslash (cygpath (scriptName));
  if (_access (windowsName.c_str(), 0) == -1)
    {
      Log (LOG_PLAIN) << "can't run " << scriptName << ": No such file"
                     << endLog;
      return -ERROR_INVALID_DATA;
    }

  int retval;
  char cmdline[CYG_PATH_MAX];

  if (sh.size() && ("sh" == scriptExtension))
    {
      sprintf (cmdline, "%s %s \"%s\"", sh.c_str(), "--norc --noprofile", scriptName.c_str());
      retval = ::run (cmdline);
    }
  else if (dash.size() && ("dash" == scriptExtension))
    {
      sprintf (cmdline, "%s \"%s\"", dash.c_str(), scriptName.c_str());
      retval = ::run (cmdline);
    }
  else if (cmd && (("bat" == scriptExtension) ||
		   ("cmd" == scriptExtension)))
    {
      sprintf (cmdline, "%s %s \"%s\"", cmd, "/c", windowsName.c_str());
      retval = ::run (cmdline);
    }
  else
    return -ERROR_INVALID_DATA;

  if (retval)
    Log (LOG_PLAIN) << "abnormal exit: exit code=" << retval << endLog;

  /* if .done file exists then delete it otherwise just ignore no file error */
  io_stream::remove ("cygfile://" + scriptName + ".done");

  /* don't rename the script as .done if it didn't run successfully or
     if this script is marked to be always run */
  if (!retval && ("p" != scriptType))
    io_stream::move ("cygfile://" + scriptName,
                     "cygfile://" + scriptName + ".done");

  return retval;
}

int
try_run_script (const std::string& dir,
                const std::string& fname,
                const std::string& ext)
{
  if (io_stream::exists ("cygfile://" + dir + fname + ext))
    return Script (dir + fname + ext).run ();
  return NO_ERROR;
}

bool
Script::isAScript (const std::string& file)
{
    // is a directory path
    if ('/'  == file[file.size()-1])
      return false;
    // file may start with /etc/postinstall/ or etc/postinstall/
    std::size_t found = file.find(ETCPostinstall+1);
    if (( found == 0) ||
	((found == 1) && (0 == file.find('/'))))
      return true;
    return false;
}

bool
Script::match (const std::string& stratum, const std::string& type)
{
  // empty string for each parameter always matches
  bool matchedStratum, matchedType;
  if ("done" == scriptExtension)
    return false;
  if (stratum.size())
    matchedStratum = (std::string::npos != stratum.find(scriptStratum));
  else
    matchedStratum = true;
  if (type.size())
    matchedType = (std::string::npos != type.find(scriptType));
  else
    matchedType = true;
  return matchedStratum && matchedType;
}
bool
Script::is_p (const std::string& stratum)
{
  return match( stratum, "p");
}
bool
Script::not_p (const std::string& stratum)
{
  return match( stratum, allowedTypes+1);
}

Script::Script (const std::string& fileName)
  : scriptName      (fileName),
    scriptBaseName  (""),
    scriptExtension (""),
    scriptStratum   ("_"),
    scriptType      ("r")
{
  std::size_t found;
  found = fileName.rfind('/');
  if (found != std::string::npos)
    scriptBaseName = fileName.substr(found + 1);
  found = fileName.rfind('.');
  if (found != std::string::npos)
    scriptExtension = fileName.substr(found + 1);
  if ( "_" == scriptBaseName.substr(2,1) &&
       0   == scriptBaseName.substr(1,1).find_first_of(allowedTypes)  &&
       "_" != scriptBaseName.substr(0,1)  && // default stratum cannot be explicitly named
       0   == scriptBaseName.substr(0,1).find_first_of(allowedStrata))
    {
      // stratified script
      scriptStratum = scriptBaseName.substr(0,1);
      scriptType    = scriptBaseName.substr(1,1);
    }
  // Let's hope people won't uninstall packages before installing bash
  init_run_script ();
}

std::string
Script::baseName() const
{
  return scriptBaseName;
}

std::string
Script::fullName() const
{
  return scriptName;
}

char const Script::ETCPostinstall[] = "/etc/postinstall/";
char const Script::allowedStrata[]  = "0_z";
char const Script::allowedTypes[]   = "pr";
