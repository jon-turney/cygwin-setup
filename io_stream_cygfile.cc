/*
 * Copyright (c) 2001, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

#include "win32.h"
#include "mklink2.h"
#include "filemanip.h"
#include "mkdir.h"
#include "mount.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "io_stream_cygfile.h"
#include "IOStreamProvider.h"
#include "LogSingleton.h"


/* completely private iostream registration class */
class CygFileProvider : public IOStreamProvider
{
public:
  int exists (const std::string& path) const
    {return io_stream_cygfile::exists(path);}
  int remove (const std::string& path) const
    {return io_stream_cygfile::remove(path);}
  int mklink (const std::string& a , const std::string& b, io_stream_link_t c) const
    {return io_stream_cygfile::mklink(a,b,c);}
  io_stream *open (const std::string& a,const std::string& b, mode_t m) const
    {return new io_stream_cygfile (a, b, m);}
  ~CygFileProvider (){}
  int move (const std::string& a,const std::string& b) const
    {return io_stream_cygfile::move (a, b);}
  int mkdir_p (path_type_t isadir, const std::string& path, mode_t mode) const
    {return cygmkdir_p (isadir, path, mode);}
protected:
  CygFileProvider() // no creating this
    {
      io_stream::registerProvider (theInstance, "cygfile://");
    }
  CygFileProvider(CygFileProvider const &); // no copying
  CygFileProvider &operator=(CygFileProvider const &); // no assignment
private:
  static CygFileProvider theInstance;
};
CygFileProvider CygFileProvider::theInstance = CygFileProvider();


std::string io_stream_cygfile::cwd("/");
  
// Normalise a unix style path relative to 
// cwd.
std::string
io_stream_cygfile::normalise (const std::string& unixpath)
{
  char *path,*tempout;
  
  if (unixpath.c_str()[0]=='/')
    {
      // rooted path
      path = new_cstr_char_array (unixpath);
      tempout = new_cstr_char_array (unixpath); // paths only shrink.
    }
  else
    {
      path = new_cstr_char_array (cwd + unixpath);
      tempout = new_cstr_char_array (cwd + unixpath); //paths only shrink.
    }
  
  // FIXME: handle .. depth tests to prevent / + ../foo/ stepping out
  // of the cygwin tree
  // FIXME: handle /./ sequences
  bool sawslash = false;
  char *outptr = tempout;
  for (char *ptr=path; *ptr; ++ptr)
    {
      if (*ptr == '/' && sawslash)
	--outptr;
      else if (*ptr == '/')
	sawslash=true;
      else
	sawslash=false;
      *outptr++ = *ptr;
    }
  std::string rv = tempout;
  delete[] path;
  delete[] tempout;
  return rv;
}

wchar_t *
io_stream_cygfile::w_str ()
{
  if (!wname)
    {
      wname = new wchar_t [fname.size () + 7];
      if (wname)
	mklongpath (wname, fname.c_str (), fname.size () + 7);
    }
  return wname;
}

static void
get_root_dir_now ()
{
  if (get_root_dir ().size())
    return;
  read_mounts (std::string ());
}

io_stream_cygfile::io_stream_cygfile (const std::string& name, const std::string& mode, mode_t perms) : fp(), lasterr (0), fname(), wname (NULL)
{
  errno = 0;
  if (!name.size())
  {
    Log (LOG_TIMESTAMP) << "io_stream_cygfile: Bad parameters" << endLog;
    return;
  }

  /* do this every time because the mount points may change due to fwd/back button use...
   * TODO: make this less...manual
   */
  get_root_dir_now ();
  if (!get_root_dir ().size())
  {
    /* TODO: assign a errno for "no mount table :} " */
    Log (LOG_TIMESTAMP) << "io_stream_cygfile: Error reading mounts" << endLog;
    return;
  }

  fname = cygpath (normalise(name));
  if (mode.size ())
    {
      if (fname.rfind (".exe") != std::string::npos
	  || fname.rfind (".dll") != std::string::npos)
        perms |= 0111;	/* Make .exe and .dll always executable. */
      fp = nt_wfopen (w_str(), mode.c_str (), perms);
      if (!fp)
      {
	lasterr = errno;
	Log (LOG_TIMESTAMP) << "io_stream_cygfile: fopen(" << name << ") failed " << errno << " "
	  << strerror(errno) << endLog;
      }
    }
}

io_stream_cygfile::~io_stream_cygfile ()
{
  if (fp)
    fclose (fp);
  if (wname)
    delete [] wname;
}

/* Static members */
int
io_stream_cygfile::exists (const std::string& path)
{
  get_root_dir_now ();
  if (!get_root_dir ().size())
    return 0;

  size_t len = cygpath (normalise(path)).size () + 7;
  WCHAR wname[len];
  mklongpath (wname, cygpath (normalise(path)).c_str (), len);
  DWORD attr = GetFileAttributesW (wname);
  if (attr != INVALID_FILE_ATTRIBUTES)
    return 1;
  return 0;
}

int
io_stream_cygfile::remove (const std::string& path)
{
  if (!path.size())
    return 1;
  get_root_dir_now ();
  if (!get_root_dir ().size())
    /* TODO: assign a errno for "no mount table :} " */
    return 1;

  size_t len = cygpath (normalise(path)).size () + 7;
  WCHAR wpath[len];
  mklongpath (wpath, cygpath (normalise(path)).c_str (), len);

  unsigned long w = GetFileAttributesW (wpath);
  if (w != INVALID_FILE_ATTRIBUTES && w & FILE_ATTRIBUTE_DIRECTORY)
    {
      len = wcslen (wpath);
      WCHAR tmp[len + 10];
      wcscpy (tmp, wpath);
      int i = 0;
      do
        {
	  ++i;
	  swprintf (tmp + len, L"old-%d", i);
	}
      while (GetFileAttributesW (tmp) != INVALID_FILE_ATTRIBUTES);
      Log (LOG_TIMESTAMP) << "warning: moving directory \"" << normalise(path).c_str()
                          << "\" out of the way." << endLog;
      MoveFileW (wpath, tmp);
    }
  return io_stream::remove (std::string ("file://") + cygpath (normalise(path)).c_str());
}

/* Returns 0 for success */
int
io_stream_cygfile::mklink (const std::string& _from, const std::string& _to,
			   io_stream_link_t linktype)
{
  if (!_from.size() || !_to.size())
    return 1;
  std::string from(normalise(_from));
  std::string to (normalise(_to));
  switch (linktype)
    {
    case IO_STREAM_SYMLINK:
      // symlinks are arbitrary targets, can be anything, and are
      // not subject to translation
      return mkcygsymlink (cygpath (from).c_str(), _to.c_str());
    case IO_STREAM_HARDLINK:
      {
	/* First try to create a real hardlink. */
	if (!mkcyghardlink (cygpath (from).c_str(), cygpath (to).c_str ()))
	  return 0;

	/* If creating a hardlink failed, we're probably on a filesystem
	   which doesn't support hardlinks.  If so, we also don't care for
	   permissions for now.  The filesystem is probably a filesystem
	   which doesn't support ACLs anyway. */

	/* textmode alert: should we translate when linking from an binmode to a
	   text mode mount and vice verca?
	 */
	io_stream *in = io_stream::open (std::string ("cygfile://") + to, "rb", 0);
	if (!in)
	  {
	    Log (LOG_TIMESTAMP) << "could not open " << to
              << " for reading in mklink" << endLog;
	    return 1;
	  }
	io_stream *out = io_stream::open (std::string ("cygfile://") + from, "wb", 0644);
	if (!out)
	  {
	    Log (LOG_TIMESTAMP) << "could not open " << from
              << " for writing in mklink" << endLog;
	    delete in;
	    return 1;
	  }

	if (io_stream::copy (in, out))
	  {
	    Log (LOG_TIMESTAMP) << "Failed to hardlink " << from << "->"
              << to << " during file copy." << endLog;
	    delete in;
	    delete out;
	    return 1;
	  }
	delete in;
	delete out;
	return 0;
      }
    }
  return 1;
}


/* virtuals */

ssize_t
io_stream_cygfile::read (void *buffer, size_t len)
{
  if (fp)
    return fread (buffer, 1, len, fp);
  return 0;
}

ssize_t
io_stream_cygfile::write (const void *buffer, size_t len)
{
  if (fp)
    return fwrite (buffer, 1, len, fp);
  return 0;
}

ssize_t
io_stream_cygfile::peek (void *buffer, size_t len)
{
  if (fp)
    {
      int pos = ftell (fp);
      ssize_t rv = fread (buffer, 1, len, fp);
      fseek (fp, pos, SEEK_SET);
      return rv;
    }
  return 0;
}

long
io_stream_cygfile::tell ()
{
  if (fp)
    {
      return ftell (fp);
    }
  return 0;
}

int
io_stream_cygfile::seek (long where, io_stream_seek_t whence)
{
  if (fp)
    {
      return fseek (fp, where, (int) whence);
    }
  lasterr = EBADF;
  return -1;
}

int
io_stream_cygfile::error ()
{
  if (fp)
    return ferror (fp);
  return lasterr;
}

int
cygmkdir_p (path_type_t isadir, const std::string& _name, mode_t mode)
{
  if (!_name.size())
    return 1;
  std::string name(io_stream_cygfile::normalise(_name));
  get_root_dir_now ();
  if (!get_root_dir ().size())
    /* TODO: assign a errno for "no mount table :} " */
    return 1;
  return mkdir_p (isadir == PATH_TO_DIR ? 1 : 0, cygpath (name).c_str(), mode);
}

int
io_stream_cygfile::set_mtime (time_t mtime)
{
  if (!fname.size())
    return 1;
  if (fp)
    fclose (fp);
  long long ftimev = mtime * NSPERSEC + FACTOR;
  FILETIME ftime;
  ftime.dwHighDateTime = ftimev >> 32;
  ftime.dwLowDateTime = ftimev;
  HANDLE h;
  h = CreateFileW (w_str (), GENERIC_WRITE,
		   FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
		   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h == INVALID_HANDLE_VALUE)
    return 1;
  SetFileTime (h, 0, 0, &ftime);
  CloseHandle (h);
  return 0;
}

int
io_stream_cygfile::move (const std::string& _from, const std::string& _to)
{
  if (!_from.size() || !_to.size())
    return 1;
  std::string from (normalise(_from));
  std::string to(normalise(_to));
  get_root_dir_now ();
  if (!get_root_dir ().size())
    /* TODO: assign a errno for "no mount table :} " */
    return 1;
  return rename (cygpath (from).c_str(), cygpath (to).c_str());
}

size_t
io_stream_cygfile::get_size ()
{
  if (!fname.size() )
    return 0;
  HANDLE h;
  DWORD ret = 0;
  h = CreateFileW (w_str (), GENERIC_READ,
		   FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
		   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h != INVALID_HANDLE_VALUE)
    {
      ret = GetFileSize (h, NULL);
      CloseHandle (h);
    }
  return ret;
}
