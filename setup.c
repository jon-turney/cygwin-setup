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

static int downloaddir (const char *url);

static SA files = {NULL, 0, 0};
static FILE *logfp = NULL;
static HANDLE devnull = NULL;
static HINTERNET session = NULL;

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

      hres = sl->lpVtbl->QueryInterface (sl, &IID_IPersistFile, &pf);

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
  FILE *out;
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
	  gzclose (gzf);
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
      fclose (out);
    }
  else
    {
      warning ("Unable to write %s: %s", name, _strerror (""));
    }

  return retval;
}

static int
tarx (const char *dir, const char *fn)
{
  char *path, *dpath;
  char buffer0[2049];
  char *buffer = buffer0 + 1;
  int hpipe[2];
  HANDLE hin;
  FILE *fp;
  int filehere;

  dpath = pathcat (dir, fn);
  path = dtoupath (dpath);
  sprintf (buffer, "tar xvfUz \"%s\"", path);
  xfree (path);
  xfree (dpath);

  printf ("Installing %s\n", fn);

  if (_pipe (hpipe, 256, O_TEXT) == -1)
    return 0;

  hin = (HANDLE) _get_osfhandle (hpipe[1]);
  if (xcreate_process (0, NULL, hin, hin, buffer) == 0)
    {
      warning ("Unable to extract \"%s\": %s", fn, _strerror (""));
      return 0;
    }
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
	  s = files.array[files.index] = utodpath (s);
	}

      fprintf (logfp, "%s\n", s);
    }
  fclose (fp);

  while (++filehere <= files.index)
    if (chmod (files.array[filehere], 0777))
      warning ("Unable to reset protection on '%s' - %s\n",
	       files.array[filehere], _strerror (""));

  return 1;
}

int
recurse_dirs (const char *dir)
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
		  || strcmp (find_data.cFileName, "..") == 0)
		continue;

	      if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
		  /* && strlen(find_data.cFileName) */ )
		{
		  char *subdir = pathcat (dir, find_data.cFileName);
		  if (subdir)
		    {
		      if (!recurse_dirs (subdir))
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
	      pattern = pathcat (dir, "*.tar.gz");
	      handle = FindFirstFile (pattern, &find_data);
	      if (handle != INVALID_HANDLE_VALUE)
		{
		  int err = 0;

		  do
		    {
		      /* Skip source archives and meta-directories */
		      if (strstr (find_data.cFileName, "-src.tar.gz")
			  || strstr (find_data.cFileName, "-src-")
			  || strcmp (find_data.cFileName, ".") == 0
			  || strcmp (find_data.cFileName, "..") == 0)
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


void
setpath (const char *element)
{
  char *buffer = xmalloc (strlen (element) + 7);

  sprintf (buffer, "PATH=%s", element);
  putenv (buffer);

  xfree (buffer);
}

char *
prompt (const char *text, const char *def)
{
  char buffer[_MAX_PATH];


  printf ((def ? "%s? [%s] " : "%s? "), text, def);
  fgets (buffer, sizeof (buffer), stdin);
  buffer[strcspn (buffer, "\r\n")] = '\0';

  /* Duplicate the entered value or the default if nothing was entered. */
  return xstrdup (strlen (buffer) ? buffer : def ? def : "");
}

int
optionprompt (const char *text, SA * options)
{
  size_t n, lbound, response;
  char buf[5];

  n = 0;

  do
    {
      char *or;
      size_t base = n;
      enum
      { CONTINUE, REPEAT, ALL }
      mode;

      if (!base)
	puts (text);

      for (n = 0; n < base + SCREEN_LINES - 2 && n < options->count; ++n)
	printf ("\t%d. %s\n", n + 1, options->array[n]);

      lbound = n - (SCREEN_LINES - (base ? 2 : 3));
      if (n < options->count)
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
      printf ("Select an option from %d-%d%s: ", lbound, n, or);
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
  while (response < lbound || response > n);

  return response - 1;
}

int
geturl (const char *url, const char *file, int verbose)
{
  DWORD type, size;
  int authenticated = 0;
  int retval = 0;
  static HINTERNET connect;
  int tries = 20;
  int is_ftp = strnicmp (url, "ftp", 3) == 0;
  static int saw_first_ftp = 0;
  char connect_buffer[sizeof ("Connecting to http site...   ")];

  if (saw_first_ftp)
    verbose = 0;
  else if (verbose)
    {
      char *p;
      int n;
      
      p = strchr (url, ':');
      if (!p)
	n = 0;
      else
	n = p - url;
      sprintf (connect_buffer, "Connecting to %.*s%ssite...", n, url, n ? " " : "");
      fputs (connect_buffer, stdout);
      fflush (stdout);
      if (is_ftp)
	saw_first_ftp = 1;
    }

  for (tries = 1; tries <= 40; tries++)
    {
      DWORD flags = INTERNET_FLAG_DONT_CACHE |
		    INTERNET_FLAG_KEEP_CONNECTION |
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

	  if (1 || !is_ftp)
	    {
	      InternetCloseHandle (connect);
	      connect = NULL;
	    }
	}
    }

  return retval;
}

char *
findhref (char *buffer, char *date, size_t *filesize)
{
  char *ref;
  char *anchor = strstr (buffer, "<A");

  if (!anchor)
    anchor = strstr (buffer, "<a");

  if (!anchor)
      return 0;

  ref = strstr (anchor, "href=");

  if (!ref)
    ref = strstr (anchor, "HREF=");

  if (ref)
    {
      int eatspace;
      char *p, *q;
      char digits[20];
      char *diglast = digits + sizeof (digits) - 1;
      int len;

      if (filesize && (anchor == buffer || !isspace (anchor[-1])))
	return 0;

      ref += ref[5] == '"' ? 6 : 5;

      len = strcspn (ref, "\" >");

      ref[len] = '\0';
      if (!filesize)
	return ref;

      *filesize = 0;
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
    }

  return ref;
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
processdirlisting (const char *urlbase, const char *file)
{
  int retval;
  char buffer[256];
  static enum {UNKNOWN, ALWAYS, NEVER} download_when = {UNKNOWN};

  FILE *in = fopen (file, "rt");

  while (fgets (buffer, sizeof (buffer), in))
    {
      size_t filesize;
      char filedate[80];
      char *ref = findhref (buffer[0] ? buffer : buffer + 1, filedate, &filesize);
      char url[256];
      DWORD urlspace = sizeof (url);
      struct _stat st;

      if (!ref)
	continue;

      if (!InternetCombineUrl
	  (urlbase, ref, url, &urlspace,
	   ICU_BROWSER_MODE | ICU_ENCODE_SPACES_ONLY | ICU_NO_META))
	{
	  warning ("Unable to download from %s", ref);
	  winerror ();
	}
      else if (ref[strlen (ref) - 1] == '/')
	{
	  if (strcmp (url + strlen (url) - 2, "./") != 0)
	    downloaddir (url);
	}
      else if (strstr (url, ".tar.gz") && !strstr (url, "-src"))
	{
	  int download = 0;
	  char *filename = strrchr (url, '/') + 1;
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
		      warning ("Skipping %s\n", filename);
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
	      if (res && !filesize || !needfile (filename, filedate, filesize))
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
			fprintf (logfp, "Downloaded file size does not match (%ld).\n",
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
			  unlink (filename); 
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
  retval = feof (in);

  fclose (in);

  return retval;
}

char *
tmpfilename ()
{
  return xstrdup (tmpnam (NULL));
}

static int
downloaddir (const char *url)
{
  int retval = 0;
  char *file = tmpfilename ();

  if (geturl (url, file, 1))
  {
    retval = processdirlisting (url, file);
    unlink (file);
  }
  xfree (file);

  return retval;
}


HINTERNET opensession ()
{
  return InternetOpen ("Cygwin Setup", INTERNET_OPEN_TYPE_PRECONFIG, NULL,
		       NULL, 0);
}

int
downloadfrom (const char *url)
{
  int retval = 0;
  char *file = tmpfilename ();

  if (geturl (url, file, 1))
    {
      retval = processdirlisting (url, file);
      unlink (file);
    }

  xfree (file);

  return retval;
}

int
reverse_sort (const void *arg1, const void *arg2)
{
  return -strcmp (*(char **) arg1, *(char **) arg2);
}

int
create_uninstall (const char *wd, const char *folder, const char *shellscut,
		  const char *shortcut)
{
  int retval = 0;
  char buffer[MAX_PATH];
  clock_t start;
  HINSTANCE lib;
  
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
	  unsigned percent = 0;
	  struct _stat st;

	  files.array[++files.index] = pathcat (cwd, "bin\\cygwin.bat");
	  files.count = files.index + 1;
	  qsort (files.array, files.count, sizeof (char *), reverse_sort);

	  fprintf (uninst,
		   "@echo off\n" "%c:\n" "cd \"%s\"\n", *cwd, cwd);
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

  if (lib)
    FreeLibrary (lib);

  warning ("Done.\n");
  return retval;
}


/* Writes the startup batch file. */
int
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

	  fprintf (batch,
		   "@echo off\n"
		   "SET MAKE_MODE=unix\n"
		   "SET PATH=%s\\bin;%s\\usr\\local\\bin;%%PATH%%\n"
		   "bash\n", root, root);
	  fclose (batch);

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

		  if (create_shortcut (cmdline, shortcut))
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

char *
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
  unlink (filename);

  return retval;
}


/* Basically a mkdir -p /somedir function. */
void
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

void
xumount (const char *mountexedir, const char *unixpath)
{
  char *umount = pathcat (mountexedir, "umount");
  char buffer[1024];

  sprintf (buffer, "%s %s", umount, unixpath);
  (void) xcreate_process (1, NULL, devnull, devnull, buffer);
  xfree (umount);
}

/* This routine assumes that the cwd is the root directory. */
int
mkmount (const char *mountexedir, const char *root, const char *dospath,
	 const char *unixpath, int force)
{
  char *mount, *bslashed, *fulldospath, *p;
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

static char rev[] = " $Revision$ ";
int
main ()
{
  int retval = 1;		/* Default to error code */
  clock_t start;
  char *logpath = NULL;
  char *revn, *p;
  int fd = _open ("nul", _O_WRONLY | _O_BINARY);

  devnull = (HANDLE) _get_osfhandle (fd);

  setbuf (stdout, NULL);
  SetEnvironmentVariable ("CYGWIN", NULL);
  revn = strchr (rev, ':');
  if (!revn || (p = strchr (revn + 2, ' ')) == NULL)
    revn = "";
  else
    {
      revn = rev;
      revn[1] = '(';
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
  if (!EnumResourceNames (NULL, "FILE", output_file, 0))
    {
      winerror ();
    }
  else
    {
      char *defroot, *update;
      char *root;
      int done;
      HKEY cu = NULL, lm = NULL;

      wd = _getcwd (NULL, 0);
      setpath (wd);

      logpath = pathcat (wd, "setup.log");

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

      /* Create the root directory. */
      mkdir (root);		/* Ignore any return value since it may
				   already exist. */
      mkmount (wd, "", root, "/", 1);

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

	  downloadfrom (dir);
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
	  if (recurse_dirs (wd))
	    {
	      char *mount;
	      char buffer[1024];

	      /* Mount the new root directory. */
	      mount = pathcat (wd, "mount");
	      sprintf (buffer, "%s -f -b \"%s\" /", mount, root);
	      xfree (mount);
	      if (xsystem (buffer))
		{
		  printf
		    ("Unable to mount \"%s\" as the root directory: %s",
		     root, _strerror (""));
		}
	      else
		{
		  char **a;
		  /* bash expects a /tmp */
		  char *tmpdir = pathcat (root, "tmp");

		  if (tmpdir)
		    {
		      files.array[++files.index] = tmpdir;
		      mkdir (tmpdir);	/* Ignore the result, it may
					       exist. */
		    }

		  files.array[++files.index] = pathcat (root, "usr\\local");
		  files.array[++files.index] = pathcat (root, "usr\\local\\bin");
		  files.array[++files.index] = pathcat (root, "usr\\local\\lib");
		  mkdirp (files.array[files.index]);
		  mkdir (files.array[files.index - 1]);

		  if (do_start_menu (root))
		    retval = 0;	/* Everything worked return
				       successful code */
		}
	    }
	}

      xfree (root);

      chdir (wd);
      _chdrive (toupper (*wd) - 'A' + 1);
      xfree (wd);
    }

  printf ("\nInstallation took %.0f seconds.\n",
          (double) (clock () - start) / CLK_TCK);

  if (logpath)
    {
      fclose (logfp);
      xfree (logpath);
    }

  return retval;
}
