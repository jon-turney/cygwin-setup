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
 * Written by Christopher Faylor <cgf@redhat.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>
#include "setup.h"
#include <sys/stat.h>

#define CYGMAJOR "%%% Cygwin dll major: "
#define CYGMAJOR_LEN (sizeof (CYGMAJOR) - 1)
#define CYGMINOR "%%% Cygwin dll minor: "
#define CYGMINOR_LEN (sizeof (CYGMINOR) - 1)

void
normalize_version (const char *fn_in, char **prod, char **version)
{
  char *p;
  char *fn, *origfn;
  char *dot;
  static char buf1[1024];
  static char buf2[1024];

  origfn = buf1;
  strcpy (buf1, fn_in);
  fn = origfn = buf1;

  if ((p = strstr (fn, ".tar.gz")) != NULL)
    *p = '\0';
  else if ((p = strstr (fn, "_tar.gz")) != NULL)
    *p = '\0';

  while (isalpha (*fn))
    fn++;
  if (*fn)
    *fn++ = '\0';

  *prod = origfn;
  if (!*fn)
    {
      *version = strcpy (buf2, "0000");
      return;
    }

  dot = "";
  buf2[0] = '\0';
  while (*fn)
    {
      int n;
      fn += strspn (fn, "-_.+,");
      if (!*fn)
	break;
      n = strcspn (fn, "-_.+,");
      sprintf (strchr (buf2, '\0'), "%s%04.*s", dot, n, fn);
      fn += n;
      dot = ".";
    }

  *version = buf2;
  return;
}

static HKEY hkpkg;

static pkg default_pkgs[] =
{
  {"diff", "0000"},
  {"ash", "0000"},
  {"bash", "0000"},
  {"binutils", "19990818.0001"},
  {"bison", "0000"},
  {"byacc", "0000"},
  {"bzip", "0000"},
  {"clear", "0001.0000"},
  {"dejagnu", "0000"},
  {"expect", "0000"},
  {"fileutils", "0000"},
  {"findutils", "0000"},
  {"flex", "0000"},
  {"gawk", "0000"},
  {"gcc", "0002.0095.0002.0001"},
  {"gdb", "20000415"},
  {"gperf", "0000"},
  {"grep", "0000"},
  {"groff", "0001.011a.0001"},
  {"gzip", "0000"},
  {"less", "0000"},
  {"m", "0000"},
  {"make", "0000"},
  {"man", "0001.005g.0001"},
  {"patch", "0000"},
  {"sed", "0000"},
  {"shellutils", "0000"},
  {"tar", "0000"},
  {"tcltk", "0000"},
  {"termcap", "0000"},
  {"texinfo", "0000"},
  {"textutils", "0000"},
  {"time", "0000"},
  {NULL, NULL}
};

pkg *
use_default_pkgs (pkg *stuff)
{
  pkg *def, *stf;
  int sawend;
  def = default_pkgs;

  sawend = 0;
  for (stf = stuff, def = default_pkgs; def->name != NULL; stf++)
    {
      if (sawend || !stf->name)
	{
	  stf->name = xstrdup (def->name);
	  stf->version = xstrdup (def->version);
	  def++;
	  sawend = 1;
	}
      (void) write_pkg (NULL, stf->name, stf->version);
    }
  stf->name = NULL;
  stf->version = NULL;
  return stuff;
}

pkg *
find_pkg (pkg *stuff, char *name)
{
  int i;

  for (i = 0; stuff[i].name; i++)
    if (stricmp (stuff[i].name, name) == 0)
      return stuff + i;

  return NULL;
}

const char *
check_for_installed (const char *root, pkg *stuff)
{
  char *cygwin = pathcat (root, "bin\\cygwin1.dll");
  FILE *fp = fopen (cygwin, "rb");
  char *buf, *bufend;
  struct _stat st;
  char *major, *minor;
  pkg *pkg;
  static char buf1[256];

  xfree (cygwin);

  if (fp == NULL)
    return NULL;

  if (_fstat (fileno (fp), &st))
    goto err;

  buf = xmalloc (st.st_size);
  if (!buf)
    goto err;

  if (fread (buf, st.st_size, 1, fp) <= 0)
    goto err;

  fclose (fp);

  bufend = buf + st.st_size;
  major = minor = NULL;
  while (buf < bufend)
    if ((buf = memchr (buf, '%', bufend - buf)) == NULL)
	return 0;
    else if (strncmp (buf, CYGMAJOR, CYGMAJOR_LEN) == 0)
	major = buf += CYGMAJOR_LEN;
    else if (strncmp (buf, CYGMINOR, CYGMINOR_LEN) != 0)
	buf++;
    else
	{
	  minor = buf + CYGMINOR_LEN;
	  break;
	}

  if (!minor)
    return NULL;

  sprintf (buf1, "%04d.%04d.%04d", atoi (major) / 1000, atoi (major) % 1000, atoi (minor));

  pkg = find_pkg (stuff, "cygwin");
  if (pkg)
    xfree (pkg->version);
  else
    {
      pkg = stuff;
      stuff[1].name = stuff[1].version = NULL;
    }

  pkg->name = xstrdup ("cygwin");
  pkg->version = xstrdup (buf1);

  sprintf (buf1, "%d.%d.%d", atoi (major) / 1000, atoi (major) % 1000, atoi (minor));

  return buf1;

err:
  fclose (fp);
  return NULL;
}

pkg *
init_pkgs (int use_current_user)
{
  LONG res;
  DWORD what;
  char empty[] = "";
  char buf[4096];
  DWORD ty, sz;
  DWORD nc = 0;
  static pkg stuff[1000];

  res = RegCreateKeyEx (use_current_user ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
			"SOFTWARE\\Cygnus Solutions\\Cygwin\\Installed Components",
			 0, empty,  REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkpkg, &what);

  if (res != ERROR_SUCCESS)
    return NULL;

  for (nc = 0, sz = sizeof (buf);
       RegEnumValue (hkpkg, nc, buf, &sz, NULL, &ty, NULL, NULL) == ERROR_SUCCESS;
       nc++, sz = sizeof (buf))
    {
      DWORD sz = sizeof (buf);
      stuff[nc].name = xstrdup (buf);

      if (RegQueryValueEx (hkpkg, stuff[nc].name, NULL,
			 &ty, buf, &sz) == ERROR_SUCCESS)
	stuff[nc].version = xstrdup (buf);
      else
	stuff[nc].version = xstrdup ("");
    }

  stuff[nc].version = stuff[nc].name = NULL;
  return stuff;
}

int
write_pkg (pkg *pkg, char *name, char *version)
{
  if (pkg != NULL && stricmp (pkg->version, version) >= 0)
    return 0;

  RegSetValueEx (hkpkg, name, 0, REG_SZ, version, strlen (version) + 1);
  return 1;
}

int
newer_pkg (pkg *pkg, char *version)
{
  if (pkg != NULL && stricmp (pkg->version, version) >= 0)
    return 0;
  return 1;
}

void
close_pkgs ()
{
  RegCloseKey (hkpkg);
}

#ifdef check
int
main (int argc, char **argv)
{
  pkg *stuff = init_pkgs ();

  while (*++argv)
    {
      char *stub, *ver;
      pkg *pkg;
      normalize_version (*argv, &stub, &ver);
      printf ("%s = %s, %s\n", *argv, stub, ver);
      if ((pkg = find_pkg (stuff, stub)) != NULL)
	printf ("found pkg %s = %s\n", pkg->name, pkg->version);
      printf ("write_pkg returns %d\n", write_pkg (pkg, stub, ver));
    }
  exit (0);
}
#endif
