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
#include "zlib/zlib.h"

char *pathcat (const char *, const char *);
char *pathcvt (char target, const char *path);
char *dtoupath (const char *path);
char *utodpath (const char *path);

char *
pathfp (char *command, int closeit)
{
  int hpipe[2];
  static FILE *in = NULL;
  char *retval;
  char buffer[1024];

  if (in == NULL)
    {
      /* If there is an error try using the original style path anyway. */
      if (_pipe (hpipe, 256, O_BINARY) == -1)
	retval = NULL;
      else
	{
	  HANDLE hpipe1 = (HANDLE) _get_osfhandle (hpipe[1]);
	  in = fdopen (hpipe[0], "r");

	  if (!in || !xcreate_process (1, NULL, hpipe1, NULL, command))
	    return NULL;
	  close (hpipe[1]);
	}
    }

  retval = fgets (buffer, sizeof (buffer), in);

  if (retval == NULL)
    closeit = 1;
  else
    {
      buffer[strcspn (buffer, "\r\n")] = '\0';
      retval = xstrdup (buffer);
    }

  if (closeit)
    {
      fclose (in);
      in = NULL;
    }

  return retval;
}
  
char *
pathcvt (char target, const char *path)
{
  char buffer[1024];
  char *retval;

  /* Get the Windows version of the root path from cygpath. */
  sprintf (buffer, "cygpath -%c \"%s\"", target, path);
  retval = pathfp (buffer, 1);
  return retval ? retval : xstrdup (path);
}

char *
dtoupath (const char *path)
{
  char *retval = pathcvt ('u', path);
  size_t len = strlen (retval);
  if (len > 2 && retval[len - 1] == '/')	/* Trim the trailing slash
						   off of a nonroot path. */
    retval[len - 1] = '\0';

  return retval;
}

char *
utodpath (const char *path)
{
  char *retval = pathcvt ('w', path);
  size_t len = strlen (retval);
  if (len > 3 && retval[len - 1] == '\\')	/* Trim the trailing slash
						   off of a nonroot path. */
    retval[len - 1] = '\0';

  return retval;
}

char *
futodpath (const char *path)
{
  char buffer[1024];
  sprintf (buffer, "cygpath -d -f %s", path);
  return pathfp (buffer, 0);
}

char *
pathcat (const char *arg1, const char *arg2)
{
  char path[_MAX_PATH];
  size_t len;

  assert (!strchr (arg1, '/'));
  strcpy (path, arg1);

  /* Remove any trailing slash */
  len = strlen (path);
  if (path[--len] == '\\')
    path[len] = '\0';

  strcat (path, "\\");

  if (*arg2 == '\\')
    ++arg2;

  strcat (path, arg2);

  return xstrdup (path);
}
