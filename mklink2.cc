#include <stdlib.h>
#include <wchar.h>
#include "win32.h"
#include "ntdll.h"
#include "shlobj.h"
#include "mklink2.h"
#include "filemanip.h"

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
      sl->QueryInterface (IID_IPersistFile, (void **) &pf);

      sl->SetPath (exepath);
      sl->SetArguments (args);
      sl->SetIconLocation (icon, 0);

      MultiByteToWideChar (CP_ACP, 0, lname, -1, widepath, MAX_PATH);
      pf->Save (widepath, TRUE);

      pf->Release ();
    }
}

#define SYMLINK_COOKIE "!<symlink>"

extern "C"
int
mkcygsymlink (const char *from, const char *to)
{
  char buf[strlen (SYMLINK_COOKIE) + 4096];
  unsigned long w;
  const size_t len = strlen (from) + 7;
  WCHAR wfrom[len];
  HANDLE h;
  SECURITY_DESCRIPTOR sd;
  acl_t acl;
  SECURITY_ATTRIBUTES sa = { sizeof (SECURITY_ATTRIBUTES),
			     nt_sec.GetPosixPerms (from, NULL, NULL, 0644,
						   sd, acl),
			     FALSE };

  mklongpath (wfrom, from, len);
  h = CreateFileW (wfrom, GENERIC_WRITE, 0, &sa, CREATE_NEW,
		   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h == INVALID_HANDLE_VALUE)
    return 1;
  strcpy (buf, SYMLINK_COOKIE);
  strncat (buf, to, 4095);
  if (WriteFile (h, buf, strlen (buf) + 1, &w, NULL))
    {
      CloseHandle (h);
      SetFileAttributesW (wfrom, FILE_ATTRIBUTE_SYSTEM);
      return 0;
    }
  CloseHandle (h);
  DeleteFileW (wfrom);
  return 1;
}

static struct {
  FILE_LINK_INFORMATION fli;
  WCHAR namebuf[32768];
} sfli;

extern "C"
int
mkcyghardlink (const char *from, const char *to)
{
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
