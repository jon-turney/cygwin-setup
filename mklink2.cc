#include <stdlib.h>
#include <wchar.h>
#include "win32.h"
#include "ntdll.h"
#include "shlobj.h"
#include "mklink2.h"
#include "filemanip.h"
#include "winioctl.h"
#include "LogSingleton.h"
#include "mount.h"

SymlinkTypeEnum symlinkType = SymlinkTypeMagic; // default to historical behaviour

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

static int
mkmagiccygsymlink (const char *from, const char *to)
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

#ifndef IO_REPARSE_TAG_LX_SYMLINK
#define IO_REPARSE_TAG_LX_SYMLINK (0xa000001d)
#endif

typedef struct _REPARSE_LX_SYMLINK_BUFFER
{
  DWORD ReparseTag;
  WORD  ReparseDataLength;
  WORD  Reserved;
  struct {
    DWORD FileType;     /* Value is apparently always 2 for symlinks. */
    char  PathBuffer[1];/* UTF-8 encoded POSIX path
                           Isn't \0 terminated.
                           Length is ReparseDataLength - sizeof (FileType).
                        */
  } LxSymlinkReparseBuffer;
} REPARSE_LX_SYMLINK_BUFFER,*PREPARSE_LX_SYMLINK_BUFFER;

static int
mkwslsymlink (const char *from, const char *to)
{
  /* Construct the reparse path */
  std::string lxsymto;
  if (to[0] == '/')
    {
      /* If 'to' is absolute and starts with '/cygdrive' or /proc/cygdrive',
         this is a problem because: (i) the cygdrive prefix might be different,
         and (ii) the target drive might not exist, on the install system.

         Because of these problems, we don't expect any install packages to have
         links like that (they should instead be created by post-install
         scripts), but fail if they do.
      */
      if ((strncmp(to, "/cygdrive", 9) == 0) ||
          (strncmp(to, "/proc/cygdrive", 14) == 0))
        {
          Log (LOG_PLAIN) << "Refusing to create WSL symlink to" << to << " as it starts with /cygdrive" << endLog;
          return 1;
        }

      /* Otherwise, we convert the absolute path 'to' into a form a WSL
         compatible form, constructed from the '/mnt' prefix and the cygwin root
         directory e.g. /mnt/c/cygwin64/ */
      lxsymto = "/mnt/";
      std::string root = get_root_dir();
      if (root[1] == ':')
        {
          lxsymto.append(1, tolower(root.c_str()[0]));
          lxsymto.append("/");
          lxsymto.append(&(root[3]));
        }
      else
        {
          // root dir is UNC path ???
          lxsymto.append(root.c_str());
        }
      lxsymto.append(to);
    }
  else
    {
      /* Otherwise 'to' is relative to 'from', so leave it alone */
      lxsymto = to;
    }

  /* Create reparse point. */
  SECURITY_DESCRIPTOR sd;
  acl_t acl;
  nt_sec.GetPosixPerms (from, NULL, NULL, 0644, sd, acl);

  const size_t flen = strlen (from) + 7;
  WCHAR wfrom[flen];
  mklongpath (wfrom, from, flen);
  wfrom[1] = '?';

  HANDLE fh;
  UNICODE_STRING ufrom;
  IO_STATUS_BLOCK io;
  OBJECT_ATTRIBUTES attr;
  RtlInitUnicodeString (&ufrom, wfrom);
  InitializeObjectAttributes (&attr, &ufrom, OBJ_CASE_INSENSITIVE, NULL, &sd);
  NTSTATUS status = NtCreateFile (&fh,
                         DELETE | FILE_GENERIC_WRITE | READ_CONTROL | WRITE_DAC,
                         &attr,
                         &io,
                         NULL,
                         FILE_ATTRIBUTE_NORMAL,
                         FILE_SHARE_VALID_FLAGS,
                         FILE_CREATE,
                         FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE
                         | FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT,
                         NULL, 0);
  if (!NT_SUCCESS (status))
    {
      Log (LOG_PLAIN) << "NtCreateFile status " << std::hex << status << endLog;
      return 1;
    }

  /* Set content of the reparse point */
  size_t tlen = lxsymto.length();
  REPARSE_LX_SYMLINK_BUFFER *rpl = (REPARSE_LX_SYMLINK_BUFFER *) new char[sizeof(REPARSE_LX_SYMLINK_BUFFER) + tlen];
  rpl->ReparseTag = IO_REPARSE_TAG_LX_SYMLINK;
  rpl->ReparseDataLength = sizeof (DWORD) + tlen;
  rpl->Reserved = 0;
  rpl->LxSymlinkReparseBuffer.FileType = 2;
  memcpy(rpl->LxSymlinkReparseBuffer.PathBuffer, lxsymto.c_str(), tlen);

  status = NtFsControlFile (fh, NULL, NULL, NULL, &io, FSCTL_SET_REPARSE_POINT,
                            (LPVOID) rpl,
                            REPARSE_DATA_BUFFER_HEADER_SIZE + rpl->ReparseDataLength,
                            NULL, 0);
  if (!NT_SUCCESS (status))
    {
      Log (LOG_PLAIN) << "FSCTL_SET_REPARSE_POINT status " << std::hex << status << endLog;
    }

  delete rpl;
  NtClose(fh);
  return NT_SUCCESS (status) ? 0 : 1;
}

static int
mknativesymlink (const char *from, const char *to)
{
  /* Construct the absolute Windows path of 'to' ... */
  std::string absto;
  if (to[0] == '/')
    {
      absto = get_root_dir();
      absto.append(to);
    }
  else
    {
      /* 'from' is already absolute */
      absto.append(from);
      /* remove the last pathname component */
      size_t i = absto.rfind('/');
      if (i != std::string::npos)
        absto.resize(i);
      /* ... and add relative path 'to'. */
      absto.append("/");
      absto.append(to);
    }

  /* ... so we can discover if it's a file or directory (if it already exists) */
  size_t abstlen = strlen (absto.c_str()) + 7;
  wchar_t wabsto[abstlen];
  mklongpath (wabsto, absto.c_str(), abstlen);
  wabsto[1] = '?';

  bool isdir = FALSE;
  bool isdir_known = FALSE;
  HANDLE fh;
  NTSTATUS status;
  UNICODE_STRING uto;
  OBJECT_ATTRIBUTES attr;
  IO_STATUS_BLOCK io;
  RtlInitUnicodeString (&uto, wabsto);
  InitializeObjectAttributes (&attr, &uto, OBJ_CASE_INSENSITIVE, NULL, NULL);
  status = NtOpenFile (&fh, FILE_READ_ATTRIBUTES, &attr, &io, FILE_SHARE_VALID_FLAGS,
                       FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT);
  if (NT_SUCCESS (status))
    {
      FILE_BASIC_INFORMATION fi;
      status = NtQueryInformationFile(fh, &io, &fi, sizeof(fi), FileBasicInformation);
      if (!NT_SUCCESS (status))
        Log (LOG_BABBLE) << "Querying " << absto << " failed " << std::hex << status << endLog;
      else
        {
          isdir = fi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY;
          isdir_known = TRUE;
          Log (LOG_BABBLE) << "Querying " << absto << " isdir is " << isdir << endLog;
        }
      NtClose(fh);
    }
  else
    {
      Log (LOG_BABBLE) << "Opening " << absto << " failed " << std::hex << status << endLog;
    }

  /*
    Fail, if we failed to determine if the symlink target is a directory
    (probably because it doesn't exist (yet))

    (We could guess that it's a file, since that works for Cygwin (and WSL),
    which don't care if the directory flag in the symlink is wrong (when the
    target comes into existence), but native tools will fail.
  */

  if (!isdir_known)
    return 1;

  /* Try to create the native symlink. */
  const size_t flen = strlen (from) + 7;
  WCHAR wfrom[flen];
  mklongpath (wfrom, from, flen);
  wfrom[1] = '?';

  size_t tlen = strlen (to) + 7;
  wchar_t wrelto[tlen];
  wchar_t *wto;
  if (to[0] == '/')
    {
      wto = wabsto;
      // convert back from nt namespace to win32 file namespace to use with
      // CreateSymbolicLinkW()
      wabsto[1] = '\\';
      // Some parts of Windows don't correctly handle a win32 file namespace
      // prefix in the symlink target. So, for maximum interoperability, we use
      // a short path instead, if the target path will be less than MAX_PATH.
      if (wcslen(wabsto) < (MAX_PATH + 4))
        wto = wabsto + 4;
    }
  else
    {
      mklongrelpath (wrelto, to, tlen);
      wto = wrelto;
    }

  DWORD flags = isdir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
  /* Windows 10 1703 and later allow unprivileged symlink creation when
     'Developer Mode' is on.*/
  VersionInfo v = GetVer();
  if ((v.major() > 10) ||
      ((v.major() == 10) && (v.buildNumber() >= 15063)))
    flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;

  status = CreateSymbolicLinkW (wfrom, wto, flags);

  if (!status)
    Log (LOG_PLAIN) << "Linking " << from << " to " << to << " failed " << std::hex << GetLastError() << endLog;

  return !status;
}

int
mkcygsymlink (const char *from, const char *to)
{
  if (symlinkType == SymlinkTypeWsl)
    {
      if (!mkwslsymlink (from, to))
        return 0;
    }

  if (symlinkType == SymlinkTypeNative)
    {
      if (!mknativesymlink (from, to))
        return 0;
    }

  /* fall back to magic symlink, if selected method fails */
  return mkmagiccygsymlink(from, to);
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
