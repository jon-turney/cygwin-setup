#define CINTERFACE
#include <stdlib.h>
#include "win32.h"
#include "shlobj.h"
#include "mklink2.h"
#include "filemanip.h"

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

/* This part of the code must be in C because the C++ interface to COM
doesn't work. */

extern "C"
void
make_link_2 (char const *exepath, char const *args, char const *icon, char const *lname)
{
  IShellLink *sl;
  IPersistFile *pf;
  WCHAR widepath[MAX_PATH];

  CoCreateInstance (&CLSID_ShellLink, NULL,
		    CLSCTX_INPROC_SERVER, &IID_IShellLink, (LPVOID *) & sl);
  sl->lpVtbl->QueryInterface (sl, &IID_IPersistFile, (void **) &pf);

  sl->lpVtbl->SetPath (sl, exepath);
  sl->lpVtbl->SetArguments (sl, args);
  sl->lpVtbl->SetIconLocation (sl, icon, 0);

  MultiByteToWideChar (CP_ACP, 0, lname, -1, widepath, MAX_PATH);
  pf->lpVtbl->Save (pf, widepath, TRUE);

  pf->lpVtbl->Release (pf);
  sl->lpVtbl->Release (sl);
}

#define SYMLINK_COOKIE "!<symlink>"

/* Predicate: file is not currently in existence.
 * A file race can occur otherwise.
 */
static int
mkcygsymlink_9x (const char *from, const char *to)
{
  char buf[512];
  unsigned long w;

  HANDLE h = CreateFileA (from, GENERIC_WRITE, 0, 0, CREATE_NEW,
			  FILE_ATTRIBUTE_NORMAL, 0);
  if (h == INVALID_HANDLE_VALUE)
    return 1;
  strcpy (buf, SYMLINK_COOKIE);
  strcat (buf, to);
  if (WriteFile (h, buf, strlen (buf) + 1, &w, NULL))
    {
      CloseHandle (h);
      SetFileAttributesA (from, FILE_ATTRIBUTE_SYSTEM);
      return 0;
    }
  CloseHandle (h);
  DeleteFileA (from);
  return 1;
}

static int
mkcygsymlink_nt (const char *from, const char *to)
{
  char buf[strlen (SYMLINK_COOKIE) + 4096];
  unsigned long w;
  const size_t len = strlen (from) + 7;
  WCHAR wfrom[len];

  mklongpath (wfrom, from, len);
  HANDLE h = CreateFileW (wfrom, STANDARD_RIGHTS_ALL | GENERIC_WRITE,
			  0, 0, CREATE_NEW,
			  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
			  0);
  if (h == INVALID_HANDLE_VALUE)
    return 1;
  strcpy (buf, SYMLINK_COOKIE);
  strncat (buf, to, 4095);
  if (WriteFile (h, buf, strlen (buf) + 1, &w, NULL))
    {
      nt_sec.SetPosixPerms (from, h, 0644);
      CloseHandle (h);
      SetFileAttributesW (wfrom, FILE_ATTRIBUTE_SYSTEM);
      return 0;
    }
  CloseHandle (h);
  DeleteFileW (wfrom);
  return 1;
}

extern "C"
int
mkcygsymlink (const char *from, const char *to)
{
  return IsWindowsNT () ? mkcygsymlink_nt (from, to)
			: mkcygsymlink_9x (from, to);
}
