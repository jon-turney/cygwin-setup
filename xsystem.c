#include <windows.h>
#include <wininet.h>
#include <assert.h>
#include <ctype.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <shellapi.h>
#include <shlguid.h>
#include <shlobj.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "setup.h"
#include "strarry.h"

extern char *pathcat (const char *, const char *);

int
xcreate_process (int wait, HANDLE in, HANDLE out, HANDLE err, const char *cmd)
{
  int retval;
  char *command;
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  DWORD flags = 0;
  extern char *wd;

  if (cmd[1] != ':' && strncmp (cmd, "\\\\", 2) != 0)
    command = pathcat (wd, cmd);
  else
    command = xstrdup (cmd);

  memset (&si, 0, sizeof (si));
  si.cb = sizeof (si);

  si.hStdInput = in ? in : GetStdHandle (STD_INPUT_HANDLE);
  si.hStdOutput = out ? out : GetStdHandle (STD_OUTPUT_HANDLE);
  si.hStdError = err ? err : GetStdHandle (STD_ERROR_HANDLE);
  si.dwFlags = STARTF_USESTDHANDLES;

  retval = CreateProcess (NULL, command, NULL, NULL, TRUE, 0,
			  NULL, NULL, &si, &pi);

  xfree (command);
  if (retval && wait)
    WaitForSingleObject (pi.hProcess, INFINITE);

  return retval;
}

int
xsystem (const char *cmd)
{
  return !xcreate_process (1, NULL, NULL, NULL, cmd);
}
