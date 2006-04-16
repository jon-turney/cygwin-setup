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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

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

static std::string sh;
static const char *cmd = 0;
static OSVERSIONINFO verinfo;

static const char *shells[] = {
  // Bash is guaranteed to exist, /bin/sh is not.  Besides,
  // upgrading /bin/sh requires that /bin/sh not be used.
  "/bin/bash.exe",
  "/usr/bin/bash.exe",
  0
};

void
init_run_script ()
{
  for (int i = 0; shells[i]; i++)
    {
      sh = backslash (cygpath (shells[i]));
      if (_access (sh.c_str(), 0) == 0)
	break;
      sh.clear();
    }
  
  char old_path[MAX_PATH];
  GetEnvironmentVariable ("PATH", old_path, sizeof (old_path));
  SetEnvironmentVariable ("PATH", backslash (cygpath ("/bin") + ";" +
					     cygpath ("/usr/bin") + ";" +
					     old_path).c_str());
  SetEnvironmentVariable ("CYGWINROOT", get_root_dir ().c_str());

  verinfo.dwOSVersionInfoSize = sizeof (verinfo);
  GetVersionEx (&verinfo);

  switch (verinfo.dwPlatformId)
    {
    case VER_PLATFORM_WIN32_NT:
      cmd = "cmd.exe";
      break;
    case VER_PLATFORM_WIN32_WINDOWS:
      cmd = "command.com";
      break;
    default:
      cmd = "command.com";
      break;
    }
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

  if (mkdir_p (0, backslash (cygpath (_filename)).c_str()))
    return;

  _handle = CreateFile (backslash (cygpath (_filename)).c_str(),
      GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
      &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (_handle == INVALID_HANDLE_VALUE)
    {
      log(LOG_PLAIN) << "error: Unable to redirect output to '" << _filename
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
      log(LOG_PLAIN) << "error: Unable to remove temporary file '" << _filename
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

static int
run (const char *sh, const char *args, const char *file, OutputLog &file_out)
{
  char cmdline[MAX_PATH];
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  DWORD flags = CREATE_NEW_CONSOLE;
  DWORD exitCode = 0;
  BOOL inheritHandles = FALSE;
  BOOL exitCodeValid = FALSE;

  sprintf (cmdline, "%s %s %s", sh, args, file);
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
      flags = CREATE_NO_WINDOW;  // Note: this is ignored on Win9x
    }

  BOOL createSucceeded = CreateProcess (0, cmdline, 0, 0, inheritHandles,
					flags, 0, get_root_dir ().c_str(),
					&si, &pi);

  if (createSucceeded)
    {
      WaitForSingleObject (pi.hProcess, INFINITE);
      exitCodeValid = GetExitCodeProcess(pi.hProcess, &exitCode);
    }
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  if (exitCodeValid)
    return exitCode;
  return -GetLastError();
}

char const *
Script::extension() const
{
  return strrchr (scriptName.c_str(), '.');
}

int
Script::run() const
{
  if (!extension())
    return -ERROR_INVALID_DATA;

  BOOL to_log (TRUE);
  std::string log_name;
  int retval = 0;
  if (to_log)
    {
      char tmp_pat[] = "/var/log/setup.log.postinstallXXXXXXX";
      log_name = std::string(mktemp(tmp_pat));
    }
  OutputLog file_out(log_name);

  if (sh.size() && strcmp (extension(), ".sh") == 0)
    {
      log(LOG_PLAIN) << "running: " << sh << " -c " << scriptName << endLog;
      retval = ::run (sh.c_str(), "-c", scriptName.c_str(), file_out);
    }
  else if (cmd && strcmp (extension(), ".bat") == 0)
    {
      std::string windowsName = backslash (cygpath (scriptName));
      log(LOG_PLAIN) << "running: " << cmd << " /c " << windowsName << endLog;
      retval = ::run (cmd, "/c", windowsName.c_str(), file_out);
    }
  else
    return -ERROR_INVALID_DATA;

  if (to_log && !file_out.isEmpty ())
    log(LOG_BABBLE) << file_out << endLog;

  if (retval)
    log(LOG_PLAIN) << "abnormal exit: exit code=" << retval << endLog;;

  /* if file exists then delete it otherwise just ignore no file error */
  io_stream::remove ("cygfile://" + scriptName + ".done");

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

char const Script::ETCPostinstall[] = "/etc/postinstall/";

bool
Script::isAScript (const std::string& file)
{
    /* file may be /etc/postinstall or etc/postinstall */
    if (casecompare(file, ETCPostinstall, sizeof(ETCPostinstall)-1) &&
	casecompare(file, ETCPostinstall+1, sizeof(ETCPostinstall)-2))
      return false;
    if (file.c_str()[file.size() - 1] == '/')
      return false;
    return true;
}

Script::Script (const std::string& fileName) : scriptName (fileName)
{
  
}

std::string
Script::baseName() const
{
  std::string result = scriptName;
  result = result.substr(result.rfind('/') + 1);
  return result;
}

std::string
Script::fullName() const
{
  return scriptName;
}
