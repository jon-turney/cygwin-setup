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
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

#include <windows.h>
#include <stdio.h>

static HINSTANCE cygwin_dll = 0;
static char *cygwin_dll_name = 0;

static void (*cygwin_dll_init_proc) ();
static int (*mount_proc) (const char *win32_path, const char *posix_path, unsigned flags);
static int (*umount_proc) (const char *posix_path);
static int (*cygwin_umount_proc) (const char *posix_path, unsigned flags);
static FILE * (*setmntent_proc) (const char *, const char *);
static struct mntent * (*getmntent_proc) (FILE *);
static int (*endmntent_proc) (FILE *);

static void
one (void *fp, char *name)
{
  int a;
  *(int *)fp = a = (int) GetProcAddress (cygwin_dll, name);
  if (!a)
    {
      fprintf (stderr, "error: unable to find `%s' in %s\n", name, cygwin_dll_name);
      exit (1);
    }
}

int
cygcall_load_dll (char *name)
{
  /* This forces the dll to use a private "shared" area, avoiding version conflicts */
  SetEnvironmentVariable("CYGWIN_TESTING", "1");

  cygwin_dll = LoadLibrary (name);
  if (cygwin_dll == 0)
    return 1;
  cygwin_dll_name = name;

  one (&cygwin_dll_init_proc, "cygwin_dll_init");
  one (&mount_proc, "mount");
  one (&umount_proc, "umount");
  one (&cygwin_umount_proc, "cygwin_umount");
  one (&setmntent_proc, "setmntent");
  one (&getmntent_proc, "getmntent");
  one (&endmntent_proc, "endmntent");

  cygwin_dll_init_proc ();

  return 0;
}

int
cygcall_unload_dll ()
{
  FreeLibrary (cygwin_dll);
}

int
mount (const char *win32_path, const char *posix_path, unsigned flags)
{
  return mount_proc (win32_path, posix_path, flags);
}

int
umount (const char *posix_path)
{
  return umount_proc (posix_path);
}

int
cygwin_umount (const char *posix_path, unsigned flags)
{
  return cygwin_umount_proc (posix_path, flags);
}

FILE *
setmntent (const char *__filep, const char *__type)
{
  return setmntent_proc (__filep, __type);
}

struct mntent *
getmntent (FILE *__filep)
{
  return getmntent_proc (__filep);
}

int
endmntent (FILE *__filep)
{
  return endmntent_proc (__filep);
}

