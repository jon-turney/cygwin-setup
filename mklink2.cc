#define CINTERFACE
#include <stdlib.h>
#include <wchar.h>
#include "win32.h"
#include "ntdll.h"
#include "shlobj.h"
#include "mklink2.h"
#include "filemanip.h"

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

/* This part of the code must be in C because the C++ interface to COM
doesn't work. */

/* Initialized in WinMain.  This is required under Windows 7.  If 
   CoCreateInstance gets called from here, it fails to create the
   instance with an undocumented error code 0x80110474.
   FIXME: I have no idea why this happens. */
IShellLink *sl;

extern "C"
void
make_link_2 (char const *exepath, char const *args, char const *icon, char const *lname)
{
  IPersistFile *pf;
  WCHAR widepath[MAX_PATH];
  if (sl)
    {
      sl->lpVtbl->QueryInterface (sl, &IID_IPersistFile, (void **) &pf);

      sl->lpVtbl->SetPath (sl, exepath);
      sl->lpVtbl->SetArguments (sl, args);
      sl->lpVtbl->SetIconLocation (sl, icon, 0);

      MultiByteToWideChar (CP_ACP, 0, lname, -1, widepath, MAX_PATH);
      pf->lpVtbl->Save (pf, widepath, TRUE);

      pf->lpVtbl->Release (pf);
    }
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

struct {
  FILE_LINK_INFORMATION fli;
  WCHAR namebuf[32768];
} sfli;

extern "C"
int
mkcyghardlink (const char *from, const char *to)
{
  if (!IsWindowsNT ())
    return 1;

  size_t flen = strlen (from) + 7;
  size_t tlen = strlen (to) + 7;
  wchar_t wfrom[flen];
  wchar_t wto[tlen];
  mklongpath (wfrom, from, flen);
  wfrom[1] = '?';
  mklongpath (wto, to, tlen);
  wto[1] = '?';
  
  HANDLE fh;
  NTSTATUS status;
  UNICODE_STRING uto;
  OBJECT_ATTRIBUTES attr;
  IO_STATUS_BLOCK io;

  /* Open the existing file. */
  RtlInitUnicodeString (&uto, wto);
  InitializeObjectAttributes (&attr, &uto, OBJ_CASE_INSENSITIVE, NULL, NULL);
  status = NtOpenFile (&fh, READ_CONTROL, &attr, &io, FILE_SHARE_VALID_FLAGS,
		       FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT);
  if (!NT_SUCCESS (status))
    return 1;
  /* Create from as link to to. */
  flen = wcslen (wfrom) * sizeof (WCHAR);
  ULONG size = sizeof (FILE_LINK_INFORMATION) + flen;
  sfli.fli.ReplaceIfExists = TRUE;
  sfli.fli.RootDirectory = NULL;
  sfli.fli.FileNameLength = flen;
  memcpy (sfli.fli.FileName, wfrom, flen);
  status = NtSetInformationFile (fh, &io, &sfli.fli, size, FileLinkInformation);
  NtClose (fh);
  return NT_SUCCESS (status) ? 0 : 1;
}
