/*
 * Copyright (c) 2000, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Ron Parker <parkerrd@hotmail.com>
 *
 */

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
#include <stdarg.h>
#include <signal.h>
#include <process.h>

#include "setup.h"
#include "strarry.h"
#include "zlib/zlib.h"

#define CYGNUS_KEY "Software\\Cygnus Solutions"
#define DEF_ROOT "C:\\cygwin"
#define DOWNLOAD_SUBDIR "latest/"
#define SCREEN_LINES 25
#define COMMAND9X "command.com /E:4096 /c "

#ifndef NFILE_LIST
#define NFILE_LIST 10000
#endif

#ifndef NFILE_SLOP
#define NFILE_SLOP 20
#endif

char *wd = NULL;
static char *tarpgm;

static int downloaddir (SA *installme, const char *url);

static SA files = {NULL, 0, 0};
static FILE *logfp = NULL;
static HANDLE devnull = NULL;
static HINTERNET session = NULL;
static SA deleteme = {NULL, 0, 0};
static pkg *pkgstuff;
static int updating = 0;
static SA installme = {NULL, 0, 0};
static HANDLE hMainThread;

static void
cleanup (void)
{
  int i, j;
  extern void exit_cygpath (void);
  exit_cygpath ();
  for (i = deleteme.count; --i >= 0; )
    for (j = 0; !DeleteFile (deleteme.array[i]) && j < 20; j++)
      Sleep (100);
  sa_cleanup (&deleteme);
}

static void
cleanup_on_signal (int sig)
{
  fprintf (stderr, "\n*Exit*\r\n");
  SuspendThread (hMainThread);
  cleanup ();
  /* I have no idea why this is necessary but, without this, code
     seems to continue executing in the main thread even after
     the ExitProcess??? */
  TerminateThread (hMainThread, 1);
  ExitProcess (1);
}

static int
istargz (char *fn)
{
  int len = strlen (fn);

  if (len > sizeof (".tar.gz") &&
      stricmp (fn + len - (sizeof ("tar.gz") - 1), "tar.gz") == 0)
    return 1;

  return 0;
}

void
warning (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  vfprintf (stderr, fmt, args);
  if (logfp)
    vfprintf (logfp, fmt, args);
}

static int
create_shortcut (const char *target, const char *shortcut)
{
  HRESULT hres;
  IShellLink *sl;
  char *path, *args;

  if (!SUCCEEDED (CoInitialize (NULL)))
    return 0;

  hres =
    CoCreateInstance (&CLSID_ShellLink, NULL,
		      CLSCTX_INPROC_SERVER, &IID_IShellLink, (LPVOID *) & sl);
  if (SUCCEEDED (hres))
    {
      IPersistFile *pf;
      int quoted = 0;
      char *c;

      /* Get the command only. */
      path = xstrdup (target);
      for (c = path; quoted || (*c != ' ' && *c); ++c)
	{
	  if (*c == '\"')
	    quoted = !quoted;
	}
      if (*c)
	{
	  *c = '\0';
	  args = c + 1;
	}
      else
	args = "";

      sl->lpVtbl->SetPath (sl, path);
      sl->lpVtbl->SetArguments (sl, args);
      xfree (path);

      hres = sl->lpVtbl->QueryInterface (sl, &IID_IPersistFile, (void **) &pf);

      if (SUCCEEDED (hres))
	{
	  WCHAR widepath[_MAX_PATH];

	  // Ensure that the string is Unicode.
	  MultiByteToWideChar (CP_ACP, 0, shortcut, -1, widepath, MAX_PATH);

	  // Save the link by calling IPersistFile::Save.
	  hres = pf->lpVtbl->Save (pf, widepath, TRUE);
	  pf->lpVtbl->Release (pf);
	}
      sl->lpVtbl->Release (sl);
    }

  CoUninitialize ();

  return SUCCEEDED (hres);
}

BOOL CALLBACK
output_file (HMODULE h, LPCTSTR type, LPTSTR name, LONG lparam)
{
  HRSRC rsrc;
  HGLOBAL res;
  char *data;
  FILE *out = NULL;
  BOOL retval = FALSE;

  size_t bytes_needed;
  if ((rsrc = FindResource (NULL, name, "FILE"))
      && (res = LoadResource (NULL, rsrc))
      && (data = (char *) LockResource (res)) && (out = fopen (strlwr (name), "w+b")))
    {
      gzFile gzf;
      char *buffer;
      size_t bytes = SizeofResource (NULL, rsrc);

      if (bytes != fwrite (data, 1, bytes, out))
	warning ("Unable to write %s: %s", name, _strerror (""));

      bytes_needed = *(int *) ((char *) data + bytes - sizeof (int));
      buffer = (char *) xmalloc (bytes_needed);

      rewind (out);
      gzf = gzdopen (_dup (fileno (out)), "rb");
      if (gzf && (size_t) gzread (gzf, buffer, bytes_needed) == bytes_needed)
	{
	  if (fseek (out, 0, SEEK_SET)
	      || fwrite (buffer, 1, bytes_needed, out) != bytes_needed)
	    {
	      warning ("Unable to write decompressed file to %s: %s",
		      name, _strerror (""));
	    }
	  else
	    retval = TRUE;
	}
      else
	{
	  int errnum;
	  const char *msg = gzerror (gzf, &errnum);
	  warning ("bytes_needed = %d, ", bytes_needed);
	  warning ("Unable to decompress %s: Error #%d, %s\n", name,
		  errnum, msg);
	}
      xfree (buffer);
      if (gzf)
	gzclose (gzf);
      fclose (out);
      sa_add (&deleteme, name);
    }
  else
    {
      warning ("Unable to write %s: %s", name, _strerror (""));
    }

  return retval;
}

static void
xumount (const char *mountexedir, const char *unixpath)
{
  char *umount = pathcat (mountexedir, "umount");
  char buffer[1024];

  sprintf (buffer, "%s %s", umount, unixpath);
  (void) xcreate_process (1, NULL, devnull, devnull, buffer);
  xfree (umount);
}

static int
tarx (const char *dir, const char *fn)
{
  char *path, *dpath;
  char buffer0[2049];
  char *buffer = buffer0 + 1;
  int hpipe[2];
  HANDLE hin;
  HANDLE hproc;
  FILE *fp;
  int filehere;
  char *pkgname, *pkgversion;
  pkg *pkg;

  normalize_version (fn, &pkgname, &pkgversion);
  pkg = find_pkg (pkgstuff, pkgname);
  if (!newer_pkg (pkg, pkgversion))
    {
      warning ("Skipped  extraction of package '%s'\n", pkgname);
      return 1;
    }

  dpath = pathcat (dir, fn);
  path = dtoupath (dpath);
  sprintf (buffer, "%s xvfUz \"%s\"", tarpgm, path);
  xfree (path);
  xfree (dpath);

  printf ("Installing %s\n", fn);

  if (_pipe (hpipe, 256, O_TEXT) == -1)
    return 0;

  hin = (HANDLE) _get_osfhandle (hpipe[1]);
  hproc = (HANDLE) xcreate_process (0, NULL, hin, hin, buffer);
  if (!hproc)
    {
      warning ("Unable to extract \"%s\": %s", fn, _strerror (""));
      return 0;
    }

  CloseHandle (hproc);
  _close (hpipe[1]);
  fp = fdopen (hpipe[0], "rt");

  filehere = files.index;
  while (fgets (buffer, sizeof (buffer0), fp))
    {
      char *s = strchr (buffer, '\n');

      if (s)
	*s = '\0';

      if (strchr (buffer, ':') != NULL)
	{
	  s = buffer;
	  fprintf (stderr, "%s\n", s);
	}
      else
	{
	  if (++files.index >= files.count)
	    files.array = realloc (files.array,
				   NFILE_SLOP + (files.count += NFILE_LIST));
	  s = buffer;
	  if (*s != '/')
	    *--s = '/';
#if 0 /* too simplistic */
	  e = strchr (s, '\0') - 1;
	  if (e > s && *e == '/')
	    {
	      *e = '\0';
	      xumount (wd, s);
	    }
#endif

	  s = files.array[files.index] = utodpath (s);
	}

      fprintf (logfp, "%s\n", s);
    }
  fclose (fp);

  while (++filehere <= files.index)
    if (chmod (files.array[filehere], 0777))
      warning ("Unable to reset protection on '%s' - %s\n",
	       files.array[filehere], _strerror (""));

  /* Use the version of the cygwin that was just installed rather than the
     tar file name.  (kludge) */
  if (stricmp (pkgname, "cygwin") == 0)
    (void) check_for_installed (".", pkgstuff);

  warning ("%s package '%s'\n", write_pkg (pkg, pkgname, pkgversion) ?
			      "Updated" : "Refreshed", pkgname);

  return 1;
}

static int
refmatches (SA *installme, char *ref, char *refend)
{
  int i;
  char *p, *q;
  char filebuf[4096];
  if (!installme->count)
    return 1;

  for (p = ref; (q = strchr (p, '/')) != refend; p = q + 1)
    continue;

  strcpy (filebuf, p);
  strchr (filebuf, '\0')[-1] = '\0';
  for (i = 0; i < installme->count; i++)
    if (stricmp (installme->array[i], filebuf) == 0)
      return 1;

  return 0;
}

static int
filematches (SA *installme, char *fn)
{
  int i;

  if (!installme->count)
    return 1;

  for (i = 0; i < installme->count; i++)
    if (strnicmp (installme->array[i], fn, strlen (installme->array[i])) == 0)
      return 1;

  return 0;
}

static int
recurse_dirs (SA *installme, const char *dir)
{
  int err = 0;
  int retval = 0;

  char *pattern = pathcat (dir, "*");
  if (pattern)
    {
      WIN32_FIND_DATA find_data;
      HANDLE handle;

      handle = FindFirstFile (pattern, &find_data);
      if (handle != INVALID_HANDLE_VALUE)
	{
	  /* Recurse through all subdirectories */
	  do
	    {
	      if (strcmp (find_data.cFileName, ".") == 0
		  || strcmp (find_data.cFileName, "..") == 0
		  || !filematches (installme, find_data.cFileName))
		continue;

	      if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
		  /* && strlen(find_data.cFileName) */ )
		{
		  char *subdir = pathcat (dir, find_data.cFileName);
		  if (subdir)
		    {
		      if (!recurse_dirs (installme, subdir))
			{
			  xfree (subdir);
			  err = 1;
			  break;
			}

		      xfree (subdir);
		    }
		  else
		    lowmem ();
		}
	    }
	  while (FindNextFile (handle, &find_data) && !err);
	  FindClose (handle);

	  /* Look for .tar.gz files */
	  if (!err)
	    {
	      xfree (pattern);
	      pattern = pathcat (dir, "*.gz");
	      handle = FindFirstFile (pattern, &find_data);
	      if (handle != INVALID_HANDLE_VALUE)
		{
		  int err = 0;

		  do
		    {
		      /* Skip source archives and meta-directories */
		      if (strstr (find_data.cFileName, "-src") != 0
			  || strcmp (find_data.cFileName, ".") == 0
			  || strcmp (find_data.cFileName, "..") == 0
			  || !istargz (find_data.cFileName)
			  || !filematches (installme, find_data.cFileName))
			{
			  continue;
			}

		      if (!tarx (dir, find_data.cFileName))
			{
			  err = 1;
			  break;
			}
		    }
		  while (FindNextFile (handle, &find_data));
		  FindClose (handle);
		}
	      if (!err)
		retval = 1;
	    }
	}

      xfree (pattern);
    }
  else
    lowmem ();

  return retval;
}

static void
setpath (const char *element)
{
  char *buffer = xmalloc (strlen (element) + 7);

  sprintf (buffer, "PATH=%s", element);
  putenv (buffer);

  xfree (buffer);
}

static char *
prompt (const char *text, const char *def)
{
  char buffer[_MAX_PATH];

  printf ((def ? "%s? [%s] " : "%s? "), text, def);
  fflush (stdout);
  fgets (buffer, sizeof (buffer), stdin);
  buffer[strcspn (buffer, "\r\n")] = '\0';

  /* Duplicate the entered value or the default if nothing was entered. */
  return xstrdup (strlen (buffer) ? buffer : def ? def : "");
}

static int
optionprompt (const char *text, SA * options)
{
  size_t n, response = -1;
  char buf[5];
  size_t base;

  n = 0;

  do
    {
      char *or;
      enum
      { CONTINUE, REPEAT, ALL }
      mode;

      base = n;
      if (!base)
	puts (text);

      for (n = 0; n < SCREEN_LINES - 2 && (n + base) < options->count; ++n)
	printf ("\t%d. %s\n", n + 1, options->array[n + base]);

      if ((n + base) < options->count)
	{
	  mode = CONTINUE;
	  or = " or [continue]";
	}
      else if (options->count > SCREEN_LINES - 2)
	{
	  mode = REPEAT;
	  or = " or [repeat]";
	}
      else
	{
	  mode = ALL;
	  or = "";
	}
      printf ("Select an option from 1-%d%s: ", n, or);
      if (!fgets (buf, sizeof (buf), stdin))
	continue;

      if (mode == CONTINUE && (!isalnum (*buf) || strchr ("cC", *buf)))
	continue;
      else if (mode == REPEAT && (!isalnum (*buf) || strchr ("rR", *buf)))
	{
	  n = 0;
	  continue;
	}

      response = atoi (buf);
    }
  while (response < 1 || response > n);

  return base + response - 1;
}

static int
geturl (const char *url, const char *file, int verbose)
{
  DWORD type, size;
  int authenticated = 0;
  int retval = 0;
  static HINTERNET connect;
  int tries = 20;
  int is_ftp = strnicmp (url, "ftp", 3) == 0;
  static int saw_first_ftp = 0;
  char connect_buffer[1024];

  if (saw_first_ftp)
    verbose = 0;
  else if (verbose)
    {
      const char *hosthere, *hostend;
      int n;

      hosthere = strstr (url, "//");
      if (!hosthere)
	hosthere = url;	/* huh? */
      else
	hosthere += 2;

      hostend = strchr (hosthere + 1, '/');
      if (!hostend)
	hostend = strchr (hosthere + 1, '\0');
      n = hostend - hosthere;
      sprintf (connect_buffer, "Connecting to %.*s...", n, hosthere);
      fputs (connect_buffer, stdout);
      fflush (stdout);
      if (is_ftp)
	saw_first_ftp = 1;
    }

  for (tries = 1; tries <= 40; tries++)
    {
      DWORD flags = INTERNET_FLAG_DONT_CACHE |
		    INTERNET_FLAG_KEEP_CONNECTION |
		    INTERNET_FLAG_PRAGMA_NOCACHE |
		    INTERNET_FLAG_RELOAD;

      if (is_ftp)
	flags |=    INTERNET_FLAG_EXISTING_CONNECT |
		    INTERNET_FLAG_PASSIVE;

      connect = InternetOpenUrl (session, url, NULL, 0, flags, 0);
      if (connect)
	break;
      if (!verbose || tries == 1)
	/* nothing */;
      else if (tries > 2)
	printf ("\r%s(try %d)  \b\b", connect_buffer, tries);
      else
	printf ("\r%s(try %d)", connect_buffer, tries);
    }

  if (!connect)
    {
      puts ("\nCouldn't connect to ftp site."); fflush (stdout);
      winerror ();
    }
  else
    {
      if (verbose)
	{
	  if (tries > 1)
	    printf ("\r%s        \b\b\b\b\b\b\b\b", connect_buffer);
	  printf ("Done.\n"); fflush (stdout);
	}
      while (!authenticated)
	{
	  size = sizeof (type);
	  if (!InternetQueryOption
	      (connect, INTERNET_OPTION_HANDLE_TYPE, &type, &size))
	    {
	      winerror ();
	      return 0;
	    }
	  else
	    switch (type)
	      {
	      case INTERNET_HANDLE_TYPE_HTTP_REQUEST:
	      case INTERNET_HANDLE_TYPE_CONNECT_HTTP:
		size = sizeof (DWORD);
		if (!HttpQueryInfo
		    (connect, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
		     &type, &size, NULL))
		  {
		    winerror ();
		    return 0;
		  }
		else if (type == HTTP_STATUS_PROXY_AUTH_REQ)
		  {
		    DWORD len;

		    if (!InternetQueryDataAvailable (connect, &len, 0, 0))
		      {
			winerror ();
			return 0;
		      }
		    else
		      {
			char *user, *password;

			/* Have to read any pending data, WININET peculiarity. */
			char *buffer = xmalloc (len);
			do
			  {
			    InternetReadFile (connect, buffer, len, &size);
			  }
			while (size);
			xfree (buffer);

			puts ("Proxy authentication is required.\n");

			user = prompt ("Proxy username", NULL);
			if (!InternetSetOption
			    (connect, INTERNET_OPTION_PROXY_USERNAME, user,
			     strlen (user)))
			  {
			    xfree (user);
			    winerror ();
			    return 0;
			  }
			else
			  {
			    xfree (user);
			    password = prompt ("Proxy password", NULL);
			    if (!InternetSetOption
				(connect, INTERNET_OPTION_PROXY_PASSWORD,
				 password,
				 strlen (password))
				|| !HttpSendRequest (connect, NULL, 0, NULL, 0))
			      {
				xfree (password);
				winerror ();
				return 0;
			      }
			    xfree (password);
			  }
		      }
		  }
		else if (type != HTTP_STATUS_OK)
		  {
		    warning ("Error retrieving \"%s\".\n", url);
		    return 0;
		  }
		else
		  authenticated = 1;
		break;

	      default:
		authenticated = 1;
		break;
	      }

	  /* Now that authentication is complete read the file. */
	  if (!InternetQueryDataAvailable (connect, &size, 0, 0))
	    winerror ();
	  else
	    {
	      char *buffer = xmalloc (size);

	      FILE *out = fopen (file, "wb");
	      if (!out)
		warning ("Unable to open \"%s\" for output: %s\n", file,
			_strerror (""));
	      else
		{
		  for (;;)
		    {
		      DWORD readbytes;

		      if (!InternetReadFile (connect, buffer, size, &readbytes))
			winerror ();
		      else if (!readbytes)
			{
			  retval = 1;
			  break;
			}
		      else if (fwrite (buffer, 1, readbytes, out) != readbytes)
			{
			  warning ("Error writing \"%s\": %s\n", file,
				  _strerror (""));
			  break;
			}
		    }
		  fclose (out);
		}
	      xfree (buffer);
	    }

	  InternetCloseHandle (connect);
	  connect = NULL;
	}
    }

  return retval;
}

static char *
findhref (char *buffer, char *date, size_t *filesize)
{
  char *ref = NULL;
  char *anchor = NULL;
  char *p;
  int eatspace;
  char *q;
  char digits[20];
  char *diglast = digits + sizeof (digits) - 1;
  int len;

  p = buffer;
  while ((p = strchr (p, '<')) != NULL)
    {
      char *q = p;
      char ch = *++p;

      if (tolower (ch) != 'a')
	continue;

      ch = *++p;
      if (!isspace (ch))
	continue;

      for (++p; isspace (*p); p++)
	continue;

      if (strnicmp (p, "href=", 5) == 0)
	{
	  ref = p;
	  anchor = q;
	}
    }

  if (!ref)
    return NULL;

  ref += ref[5] == '"' ? 6 : 5;

  len = strcspn (ref, "\" >");

  ref[len] = '\0';
  if (!filesize)
    goto out;

  *filesize = 0;

  if (anchor == buffer || !isspace (anchor[-1]))
    goto out;

  eatspace = 1;
  *diglast = '\0';
  for (p = anchor, q = diglast; --p >= buffer; )
    if (!isspace (*p))
      {
	eatspace = 0;
	if (isdigit (*p))
	  *--q = *p;
      }
    else if (!eatspace)
      break;

  if (q < diglast)
    *filesize = atoi (q);

out:
  if (!*ref)
    return NULL;
  /* This effectively disallows using a ';' in a file name.  Hopefully,
     this will not be an issue. */
  if ((p = strrchr (ref, ';')) != NULL)
    *p = '\0';
  return *ref ? ref : NULL;
}

static int
needfile (const char *filename, char *date, size_t filesize)
{
  struct _stat st;

  if (!filesize || _stat (filename, &st))
    return 1;	/* file doesn't exist or is somehow not accessible */
  return st.st_size != filesize;
}

static int
processdirlisting (SA *installme, const char *urlbase, const char *file)
{
  int retval = 0;
  char buffer[4096];
  static enum {UNKNOWN, ALWAYS, NEVER} download_when = {UNKNOWN};
  size_t urllen = strlen (urlbase);

  FILE *in = fopen (file, "rt");

  while (fgets (buffer, sizeof (buffer), in))
    {
      size_t filesize;
      char filedate[80];
      char *ref = findhref (buffer[0] ? buffer : buffer + 1, filedate, &filesize);
      char url[256];
      DWORD urlspace = sizeof (url);
      char *refend;

      if (!ref || strnicmp (ref, "http:", 5) == 0)
	continue;

      if (!InternetCombineUrl
	  (urlbase, ref, url, &urlspace,
	   ICU_BROWSER_MODE | ICU_ENCODE_SPACES_ONLY | ICU_NO_META))
	{
	  warning ("Unable to download from %s", ref);
	  winerror ();
	}
      else if (strlen (url) == urllen || strnicmp (urlbase, url, urllen) != 0
	       || strstr (url, "/.") || strstr (url, "./"))
	continue;

      refend = ref + strlen (ref) - 1;
      if (*refend == '/')
	{
	  if (refmatches (installme, ref, refend))
	    retval += downloaddir (installme, url);
	}
      else if (istargz (url) && !strstr (url, "-src"))
	{
	  int download = 0;
	  char *filename = strrchr (url, '/') + 1;
	  char *pkgname, *pkgversion;
	  pkg *pkg;

	  if (!filematches (installme, filename))
	    continue;

	  retval++;

	  if (strnicmp (filename, "cygwin-20000301", sizeof ("cygwin-20000301") - 1) == 0)
	    normalize_version ("cygwin-1.1.0.tar.gz", &pkgname, &pkgversion);
	  else
	    normalize_version (filename, &pkgname, &pkgversion);
	  pkg = find_pkg (pkgstuff, pkgname);

	  if (!newer_pkg (pkg, pkgversion))
	    {
	      warning ("Skipped download of %s\n", filename);
	      continue;
	    }

	  if (download_when == ALWAYS || needfile (filename, filedate, filesize))
	    download = 1;
	  else
	    {
	      char text[_MAX_PATH];
	      char *answer;

	      if (download_when == NEVER)
		answer = xstrdup ("N");
	      else
		{
		  sprintf (text, "Replace %s from the Internet (ynAN)", filename);
		  answer = prompt (text, "y");
		}

	      if (answer)
		{
		  switch (*answer)
		    {
		    case 'a':
		    case 'A':
		      download_when = ALWAYS;
		      /* purposely fall through */
		    case 'y':
		    case 'Y':
		      download = 1;
		      break;
		    case 'N':
		      download_when = NEVER;
		      warning ("Skipped download of %s\n", filename);
		    case 'n':
		    default:
		      download = 0;
		    }
		  xfree (answer);
		}
	    }

	  if (!download)
	    continue;

	  for (;;)		/* file retrieval loop */
	    {
	      int res;
	      warning ("Downloading: %s...", filename);
	      fflush (stdout);
	      res = geturl (url, filename, 0);
	      if ((res && !filesize) || !needfile (filename, filedate, filesize))
		warning ("Done.\n");
	      else
		{
		  for (;;)	/* prompt loop */
		    {
		      char a;
		      char *answer;
		      if (!res)
			warning ("Download failed.\n");
		      else
			fprintf (logfp, "Downloaded file size does not match (%d).\n",
				 filesize);
		      answer = prompt (res ? "Downloaded file size does not match (Abort, Retry, Fail)" :
					     "Download failed (Abort, Retry, Fail)", "R");
		      a = toupper (*answer);
		      xfree (answer);
		      switch (a)
			{
			case 'R':
			  break;	/* try it again */
			case 'A':
			  exit (1);	/* abort program */
			case 'F':
			  warning ("Deleting %s.\n", filename);
			  _unlink (filename);
			  goto noget;	/* Keep trying to download the rest */
			default:
			  continue;	/* erroneous response */
			}

		      break;	/* from prompt loop */
		    }
		}

	    noget:
	      break;		/* Leave from file retrieval for loop.
				   Either we successfully downloaded the file
				   or the user said don't bother. */
	    }
	}
    }

  fflush (stdout);
  fclose (in);

  return retval;
}

static char *
tmpfilename ()
{
  return _tempnam (NULL, "su");
}

static int
downloaddir (SA *installme, const char *url)
{
  int retval = 0;
  char *file = tmpfilename ();

  if (geturl (url, file, 1))
  {
    retval = processdirlisting (installme, url, file);
    _unlink (file);
  }
  xfree (file);

  return retval;
}

static HINTERNET
opensession ()
{
  return InternetOpen ("Cygwin Setup", INTERNET_OPEN_TYPE_PRECONFIG, NULL,
		       NULL, 0);
}

static int
downloadfrom (SA *installme, const char *url)
{
  int retval = 0;
  char *file = tmpfilename ();

  if (geturl (url, file, 1))
    {
      retval = processdirlisting (installme, url, file);
      _unlink (file);
    }

  xfree (file);

  return retval;
}

static int
reverse_sort (const void *arg1, const void *arg2)
{
  return -strcmp (*(char **) arg1, *(char **) arg2);
}

static int
create_uninstall (const char *wd, const char *folder, const char *shellscut,
		  const char *shortcut)
{
  int retval = 0;

  warning ("Creating the uninstall file...");
  fflush (stdout);
  if (files.array)
    {
      size_t n;
      FILE *uninst;
      char cwd[MAX_PATH];
      char *uninstfile;

      getcwd (cwd, sizeof (cwd));
      uninstfile = pathcat (cwd, "uninst.bat");
      uninst = fopen (uninstfile, "wt");

      if (uninst)
	{
	  struct _stat st;

	  files.array[++files.index] = pathcat (cwd, "bin\\cygwin.bat");
	  files.count = files.index + 1;
	  qsort (files.array, files.count, sizeof (char *), reverse_sort);

	  fprintf (uninst,
		   "@echo off\n" "%c:\n" "cd \"%s\"\n", *cwd, cwd);
	  fprintf (uninst, "bin\\regtool remove '/HKLM/SOFTWARE/Cygnus Solutions/cygwin/Installed Components'\n");
	  for (n = 0; n < files.count; ++n)
	    {
	      char *dpath;

	      if (n && !strcmp (files.array[n], files.array[n - 1]))
		continue;

	      dpath = files.array[n];

	      if (_stat (dpath, &st) == 0 && st.st_mode & _S_IFDIR)
		fprintf (uninst, "rmdir \"%s\"\n", dpath);
	      else
		{
		  if (access (dpath, 6) != 0)
		    fprintf (uninst, "attrib -r \"%s\"\n", dpath);
		  fprintf (uninst, "del \"%s\"\n", dpath);
		}
	    }
	  fprintf (uninst,
		   "del \"%s\"\n"
		   "del \"%s\"\n"
		   "rmdir \"%s\"\n"
		   "del %s\n", shortcut, shellscut,
		   folder, uninstfile);
	  fclose (uninst);

	  create_shortcut (uninstfile, shortcut);
	}
      sa_cleanup (&files);
      retval = 1;
    }

  warning ("Done.\n");
  return retval;
}

/* Writes the startup batch file. */
static int
do_start_menu (const char *root)
{
  FILE *batch;
  char *batch_name = pathcat (root, "bin\\cygwin.bat");
  int retval = 0;

  /* Create the batch file for the start menu. */
  if (batch_name)
    {
      batch = fopen (batch_name, "wt");
      if (batch)
	{
	  LPITEMIDLIST progfiles;
	  char pfilespath[_MAX_PATH];
	  char *folder;
	  char *bindir = pathcat (root, "bin");
	  char *locbindir = pathcat (root, "usr\\local\\bin");

	  fprintf (batch,
		   "@echo off\n"
		   "SET MAKE_MODE=unix\n"
		   "SET PATH=%s;%s;%%PATH%%\n"
		   "bash\n", bindir, locbindir);
	  fclose (batch);
	  xfree (bindir);
	  xfree (locbindir);

	  /* Create a shortcut to the batch file */
	  SHGetSpecialFolderLocation (NULL, CSIDL_PROGRAMS, &progfiles);
	  SHGetPathFromIDList (progfiles, pfilespath);

	  folder = pathcat (pfilespath, "Cygnus Solutions");
	  if (folder)
	    {
	      char *shortcut;
	      mkdir (folder);	/* Ignore the result, it may exist. */

	      shortcut = pathcat (folder, "Cygwin 1.1.0.lnk");
	      if (shortcut)
		{
		  char *cmdline;
		  OSVERSIONINFO verinfo;
		  verinfo.dwOSVersionInfoSize = sizeof (verinfo);

		  /* If we are running Win9x, build a command line. */
		  GetVersionEx (&verinfo);
		  if (verinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		    cmdline = xstrdup (batch_name);
		  else
		    {
		      char *pccmd;
		      char windir[MAX_PATH];
		      GetWindowsDirectory (windir, sizeof (windir));

		      pccmd = pathcat (windir, COMMAND9X);
		      cmdline =
			xmalloc (strlen (pccmd) + strlen (batch_name) + 1);
		      strcat (strcpy (cmdline, pccmd), batch_name);
		      xfree (pccmd);
		    }

		  if (create_shortcut (cmdline, shortcut)
		      && (!updating || installme.count))
		    {
		      char *uninstscut =
			pathcat (folder, "Uninstall Cygwin 1.1.0.lnk");
		      if (uninstscut)
			{
			  if (create_uninstall
			      (wd, folder, shortcut, uninstscut))
			    retval = 1;
			  xfree (uninstscut);
			}
		    }
		  xfree (cmdline);
		  xfree (shortcut);
		}
	      xfree (folder);
	    }
	}
      xfree (batch_name);
    }
  return retval;
}

static char *
getdownloadsource ()
{
  char *retval = NULL;
  char *filename = tmpfilename ();

  /* Initialize session one time and one time only */
  session = opensession ();

  if (!session)
    {
      winerror ();
      exit (1);
    }

  if (!geturl ("http://sourceware.cygnus.com/cygwin/mirrors.html",
	       filename, 1))
    fputs ("Unable to retrieve the list of cygwin mirrors.\n", stderr);
  else
    {
      FILE *in = fopen (filename, "rt");

      if (!in)
	warning ("Unable to open %s for input.\n", filename);
      else
	{
	  size_t option;
	  int ready4urls = 0;
	  char buf[256];
	  SA urls, names;	/* These must stay sync'd. */

	  sa_init (&urls);
	  sa_init (&names);

	  while (fgets (buf, sizeof (buf), in))
	    {
	      if (!ready4urls)
		{
		  if (strstr (buf, "Mirror Sites:"))
		    ready4urls = 1;
		}
	      else
		{
		  char *ref = findhref (buf, NULL, NULL);

		  if (ref)
		    {
		      size_t len = strlen (ref);

		      if (ref[len - 1] == '/')
			{
			  char *name;
			  char *url = xmalloc (len + 13);

			  strcat (strcpy (url, ref), DOWNLOAD_SUBDIR);
			  sa_add (&urls, url);

			  /* Get just the sites name. */
			  name = strstr (url, "//");
			  if (name)
			    name += 2;
			  else
			    name = url;
			  *strchr (name, '/') = '\0';
			  sa_add (&names, url);

			  xfree (url);
			}
		    }
		}
	    }

	  fclose (in);
	  sa_add (&urls, "Other");
	  sa_add (&names, "Other");
	  option =
	    optionprompt ("Select a download location close to you:", &names);
	  if (option == urls.count - 1)
	    retval = prompt ("Download url", NULL);
	  else
	    retval = xstrdup (urls.array[option]);

	  sa_cleanup (&urls);
	  sa_cleanup (&names);
	}
    }
  _unlink (filename);

  return retval;
}

/* Basically a mkdir -p /somedir function. */
static void
mkdirp (const char *dir)
{
  if (mkdir (dir) == -1 && errno != EEXIST)
    {
      char *parent = strdup (dir);
      char *slash = strrchr (parent, '\\');

      if (slash)
	{
	  *slash = '\0';
	  mkdirp (parent);
	}

      xfree (parent);

      mkdir (dir);
    }
}

/* This routine assumes that the cwd is the root directory. */
static int
mkmount (const char *mountexedir, const char *root, const char *dospath,
	 const char *unixpath, int force)
{
  char *mount, *fulldospath;
  char buffer[1024];

  if (*root == '\0')
    fulldospath = xstrdup (dospath);
  else
    {
      xumount (mountexedir, unixpath);
      /* Make sure the mount point exists. */
      mount = utodpath (unixpath);
      mkdirp (mount);
      xfree (mount);
      fulldospath = pathcat (root, dospath);
    }

  /* Make sure the target path exists. */
  mkdirp (fulldospath);

  /* Mount the directory. */
  mount = pathcat (mountexedir, "mount");
  sprintf (buffer, "%s %s -b \"%s\" %s", mount, force ? "-f" : "",
	   fulldospath, unixpath);
  xfree (mount);
  xfree (fulldospath);

  return xcreate_process (1, NULL, NULL, NULL, buffer) != 0;
}

static pkg *
get_pkg_stuff (const char *root, int updating)
{
  const char *ver, *ans;
  pkg *pkgstuff = init_pkgs (root, 0);
  static pkg dummy = {NULL, NULL};

  if (!updating || !pkgstuff)
    return &dummy;

  if (pkgstuff->name != NULL)
    return pkgstuff;

  ver = check_for_installed (root, pkgstuff);
  if (!ver || stricmp (ver, "1.1.0") != 0)
    return &dummy;

  puts ("\nHmm.  You seem to have a previous cygwin version installed but there is no\n"
	"package version information in the registry.  This is probably due to the fact\n"
	"that previous versions of setup.exe did not update this information.");

  ans = prompt ("Should I update the registry with default information now", "y");
  puts ("");
  if (toupper (*ans) != 'Y')
    {
      warning ("Not writing default package information to the registry.\n");
      puts ("");
      return pkgstuff;
    }

  warning ("Writing default package information to the registry.\n");
  puts ("");
  return use_default_pkgs (pkgstuff);
}

static char rev[] = "$Revision$ ";

int
main (int argc, char **argv)
{
  int retval = 1;		/* Default to error code */
  clock_t start;
  char *logpath = NULL;
  char *revn, *p;
  int fd = _open ("nul", _O_WRONLY | _O_BINARY);

  while (*++argv)
    if (stricmp (*argv, "-f") == 0)
      updating = 0;
    else if (stricmp (*argv, "-u") == 0)
      updating = 1;
    else
      break;

  sa_init (&installme);
  if (*argv)
    do
      sa_add (&installme, *argv);
    while (*++argv);

  devnull = (HANDLE) _get_osfhandle (fd);

  setbuf (stdout, NULL);
  SetEnvironmentVariable ("CYGWIN", NULL);
  revn = strchr (rev, ':');
  if (!revn || (p = strchr (revn + 2, ' ')) == NULL)
    revn = "";
  else
    {
      revn--;
      memcpy (revn, " (v", 3);
      *p = ')';
      p[1] = '\0';
    }

  printf ( "\n\n\n\n"
"This is the Cygwin setup utility%s,\n"
"built on " __DATE__ " " __TIME__ ".\n\n"
"Use this program to install the latest version of the Cygwin Utilities\n"
"from the Internet.\n\n"
"Alternatively, if you already have already downloaded the appropriate files\n"
"to the current directory (and subdirectories below it), this program can use
those as the basis for your installation.\n\n"
"If you are installing from the Internet, please run this program in an empty\n"
"temporary directory.\n\n", revn);

  start = clock ();
  sa_init (&deleteme);

  if (!EnumResourceNames (NULL, "FILE", output_file, 0))
    {
      winerror ();
    }
  else
    {
      char *defroot, *update;
      char *root;
      char *tmp;
      int done;
      HKEY cu = NULL, lm = NULL;

      wd = _getcwd (NULL, 0);
      setpath (wd);
      tmp = xmalloc (sizeof ("TMP=") + strlen (wd));
      sprintf (tmp, "TMP=%s", wd);
      _putenv (tmp);

      logpath = pathcat (wd, "setup.log");
      tarpgm = pathcat (wd, "tar.exe");

      if (logpath)
	logfp = fopen (logpath, "wt");

      if (logfp == NULL)
	{
	  fprintf (stderr, "Unable to open log file '%s' for writing - %s\n",
		   logpath, _strerror (""));
	  exit (1);
	}

      /* Begin prompting user for setup requirements. */
      printf ("Press <enter> to accept the default value.\n");

      /* If some Cygnus software has been installed, assume there is a root
	 mount in the registry. Otherwise use C:\cygwin for the default root
	 directory. */
      if (RegOpenKey (HKEY_CURRENT_USER, CYGNUS_KEY, &cu) == ERROR_SUCCESS
	  || RegOpenKey (HKEY_LOCAL_MACHINE, CYGNUS_KEY,
			 &lm) == ERROR_SUCCESS)
	{
	  defroot = utodpath ("/");
	  if (cu)
	    RegCloseKey (cu);
	  if (lm)
	    RegCloseKey (lm);
	}
      else
	defroot = xstrdup (DEF_ROOT);

      DuplicateHandle (GetCurrentProcess (), GetCurrentThread (),
		       GetCurrentProcess (), &hMainThread, 0, 0,
		       DUPLICATE_SAME_ACCESS);
      atexit (cleanup);
      signal (SIGINT, cleanup_on_signal);

      /* Get the root directory and warn the user if there are any spaces in
	 the path. */
      for (done = 0; !done;)
	{
	  root = prompt ("Root directory", defroot);
	  if (strcmpi (root, wd) == 0)
	    {
	      printf ("Please do not use the current directory as the root directory.\n"
		      "You should run setup.exe from a temporary directory.\n");
	      continue;
	    }
	  if (strchr (root, ' '))
	    {
	      char *temp;
	      temp =
		prompt
		("Using spaces in the root directory path may cause problems."
		 "  Continue anyway", "no");
	      if (toupper (*temp) == 'Y')
		done = 1;
	      xfree (temp);
	    }
	  else
	    done = 1;
	}
      xfree (defroot);

      Sleep (0);

      /* Create the root directory. */
      mkdirp (root);		/* Ignore any return value since it may
				   already exist. */
      mkmount (wd, "", root, "/", 1);

      pkgstuff = get_pkg_stuff (root, updating);

      update =
	prompt ("Install from the current directory (d) or from the Internet (i)", "i");

      if (toupper (*update) == 'I')
	{
	  char *dir = getdownloadsource ();

	  if (!dir)
	    {
	      fprintf (stderr, "Couldn't connect to download site.\n");
	      exit (1);
	    }

	  if (!downloadfrom (&installme, dir))
	    {
	      warning ("Error: No files found to download.");
	      if (!installme.count)
		warning("  Choose another mirror site?\n");
	      else
		warning ("\n");
	      goto out;
	    }
	  InternetCloseHandle (session);
	  xfree (dir);
	}
      xfree (update);

      /* Make the root directory the current directory so that recurse_dirs
	 will * extract the packages into the correct path. */
      if (chdir (root) == -1)
	{
	  warning ("Unable to make \"%s\" the current directory: %s\n",
		   root, _strerror (""));
	}
      else
	{
	  FILE *fp;

	  _chdrive (toupper (*root) - 'A' + 1);

	  xumount (wd, "/usr");
	  xumount (wd, "/var");
	  xumount (wd, "/lib");
	  xumount (wd, "/bin");
	  xumount (wd, "/etc");

	  /* Make /bin point to /usr/bin and /lib point to /usr/lib. */
	  mkmount (wd, root, "bin", "/usr/bin", 1);
	  mkmount (wd, root, "lib", "/usr/lib", 1);

	  mkdirp ("var\\run");
	  /* Create /var/run/utmp */
	  fp = fopen ("var\\run\\utmp", "wb");
	  if (fp)
	    fclose (fp);

	  files.count = NFILE_LIST;
	  files.array = calloc (sizeof (char *), NFILE_LIST + NFILE_SLOP);
	  files.index = -1;

	  /* Extract all of the packages that are stored with setup or in
	     subdirectories of its location */
	  if (recurse_dirs (&installme, wd))
	    {
	      /* bash expects a /tmp */
	      char *tmpdir = pathcat (root, "tmp");

	      if (tmpdir && mkdir (tmpdir) == 0)
		files.array[++files.index] = tmpdir;

	      files.array[++files.index] = pathcat (root, "usr\\local");
	      files.array[++files.index] = pathcat (root, "usr\\local\\bin");
	      files.array[++files.index] = pathcat (root, "usr\\local\\lib");
	      files.array[++files.index] = pathcat (root, "usr\\local\\etc");
	      mkdirp (files.array[files.index]);
	      mkdir (files.array[files.index - 1]);
	      mkdir (files.array[files.index - 2]);

	      if (do_start_menu (root))
		retval = 0;	/* Everything worked return
				   successful code */
	    }
	}

      xfree (root);

      chdir (wd);
      _chdrive (toupper (*wd) - 'A' + 1);
      xfree (wd);
    }

out:
  puts ("");
  warning ("Installation took %.0f seconds.\n",
	  (double) (clock () - start) / CLK_TCK);

  if (logpath)
    {
      fclose (logfp);
      xfree (logpath);
    }

  return retval;
}
