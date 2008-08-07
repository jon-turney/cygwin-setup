/*
 * Copyright (c) 2008, Charles Wilson
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Charles Wilson <cygwin@cygwin.com>
 *
 */

#include "compress_lzma.h"

#include <stdexcept>
using namespace std;
#include <errno.h>
#include <memory.h>
#include <malloc.h>

/*
 * allocator for LzmaDec
 */
static void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
static void SzFree(void *p, void *address) { p = p; free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

/*
 * Predicate: the stream is open for read.
 */
compress_lzma::compress_lzma (io_stream * parent)
:
  original(NULL),
  owns_original(true),
  peeklen(0),
  lasterr(0),
  initializedOk(true),
  unpackSize(0),
  thereIsSize(false),
  inbuf(NULL),
  outbuf(NULL),
  outp(NULL),
  inPos(0),
  inSize(0),
  outPos(0),
  inProcessed(0),
  outProcessed(0)
{
  /* read only */
  if (!parent || parent->error())
    {
      lasterr = EBADF;
      return;
    }
  original = parent;
 
  inbuf = (unsigned char *) malloc (IN_BUF_SIZE);
  if (!inbuf)
    {
      lasterr = ENOMEM;
      return;
    }

  outbuf = (unsigned char *) malloc (OUT_BUF_SIZE);
  if (!outbuf)
    {
      lasterr = ENOMEM;
      destroy();
      return;
    }

  errno = 0;
  check_header ();                /* parse the header, and position fp */

  outPos = 0;
  inPos = 0;
  inSize = 0;
  outp = outbuf;  
  inProcessed = 0;
  outProcessed = 0;
  return;
}

ssize_t
compress_lzma::read (void *buffer, size_t len)
{

  if (!this->initializedOk)
    return -1;

  /* there is no recovery from a busted stream */
  if (this->lasterr)
    return -1;
 
  if (len == 0)
    return 0;

  /* peekbuf is layered on top of existing buffering code */
  if (this->peeklen)
    {
      ssize_t tmplen = std::min (this->peeklen, len);
      this->peeklen -= tmplen;
      memcpy (buffer, this->peekbuf, tmplen);
      memmove (this->peekbuf, this->peekbuf + tmplen, sizeof(this->peekbuf) - tmplen);
      ssize_t tmpread = read (&((char *) buffer)[tmplen], len - tmplen);
      if (tmpread >= 0)
        return tmpread + tmplen;
      else
        return tmpread;
    }

  if (this->outp < this->outbuf + this->outPos)
  /* outp - outbuf < outPos, but avoid sign/unsigned warning */
    {
      ssize_t tmplen = std::min ((size_t)(this->outbuf + this->outPos - this->outp), len);
      memcpy (buffer, this->outp, tmplen);
      this->outp += tmplen;
      ssize_t tmpread = read (&((char *) buffer)[tmplen], len - tmplen);
      if (tmpread >= 0)
        return tmpread + tmplen;
      else
        return tmpread;
    }

  size_t lenRemaining = len;
  unsigned char * bufp = (unsigned char *)buffer;
  /* if we made it here, any existing uncompressed data in outbuf
   * has been consumed, so reset outp and outPos
   */
  this->outp = this->outbuf;
  this->outPos = 0;
  do
    {
      if (this->inPos == this->inSize)
        {
          this->inSize = (size_t) this->original->read(this->inbuf, IN_BUF_SIZE);
          this->inPos = 0;
        }

      /* at this point, these two variables actually hold the number
       * of bytes *available* to be processed or filled
       */
      this->inProcessed = this->inSize - this->inPos;
      this->outProcessed = OUT_BUF_SIZE - this->outPos;


      ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
      if (this->thereIsSize && this->outProcessed > this->unpackSize)
        {
          this->outProcessed = unpackSize;
          finishMode = LZMA_FINISH_END;
        }

      ELzmaStatus status;
      int res = LzmaDec_DecodeToBuf(
              &(this->state),
              this->outbuf + this->outPos,
              &(this->outProcessed),
              this->inbuf + this->inPos,
              &(this->inProcessed),
              finishMode,
              &status);
      this->inPos += (UInt32)this->inProcessed;
      this->outPos += this->outProcessed;
      this->unpackSize -= this->outProcessed;

      ssize_t tmplen = std::min (this->outProcessed, lenRemaining);
      memcpy (bufp, outp, tmplen);
      outp += tmplen;
      bufp += tmplen;
      lenRemaining -= tmplen;

      if (res != SZ_OK)
        {
          lasterr = res;
          return -1;
        }

     
      if (this->thereIsSize && this->unpackSize == 0)
        {
          /* expected EOF */
          break;
        }
      if (this->inProcessed == 0 && this->outProcessed == 0)
        {
          if (this->thereIsSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
            {
              lasterr = EIO;
              return -1;
            }
          break;
        }
      if (lenRemaining == 0)
        {
          /* fulfilled request */
          break;
        }
    }
  while (true);
     
  return (len - lenRemaining); 
}

ssize_t
compress_lzma::write (const void *buffer, size_t len)
{
  throw new logic_error("compress_lzma::write is not implemented");
}

ssize_t
compress_lzma::peek (void *buffer, size_t len)
{
  /* can only peek 512 bytes */
  if (len > 512)
    return ENOMEM;

  if (len > this->peeklen)
    {
      size_t want = len - this->peeklen;
      ssize_t got = read (&(this->peekbuf[peeklen]), want);
      if (got >= 0)
        this->peeklen += got;
      else
        /* error */
        return got;
      /* we may have read less than requested. */
      memcpy (buffer, this->peekbuf, this->peeklen);
      return this->peeklen;
    }
  else
    {
      memcpy (buffer, this->peekbuf, len);
      return len;
    }
  return 0;
}

long
compress_lzma::tell ()
{
  throw new logic_error("compress_lzma::tell is not implemented");
}

int
compress_lzma::seek (long where, io_stream_seek_t whence)
{
  throw new logic_error("compress_lzma::seek is not implemented");
}

int
compress_lzma::error ()
{
  return lasterr;
}

int
compress_lzma::set_mtime (int time)
{
  if (original)
    return original->set_mtime (time);
  return 1;
}

int
compress_lzma::get_mtime ()
{
  if (original)
      return original->get_mtime ();
  return 0;
}

void
compress_lzma::release_original ()
{
  owns_original = false;
}

void
compress_lzma::destroy ()
{
  if (this->initializedOk)
  {
    LzmaDec_Free(&(this->state), &g_Alloc);
  }
  if (this->inbuf)
    {
      free (this->inbuf);
      this->inbuf = NULL;
    }
  if (this->outbuf)
    {
      free (this->outbuf);
      this->outbuf = NULL;
    }

  if (original && owns_original)
    delete original;
}

compress_lzma::~compress_lzma ()
{
  destroy ();
}

/* ===========================================================================
 *  Check the header of a lzma_stream opened for reading.
 *  IN assertion:
 *     the stream s has already been created sucessfully
 *     this method is called only once per stream
 */
void
compress_lzma::check_header ()
{
  this->initializedOk = false;
  this->thereIsSize = false;
  this->unpackSize = 0;

  /* read properties */
  if (this->original->read(this->header, sizeof(this->header)) != sizeof(this->header))
    {
      this->lasterr = (errno ? errno : EIO);
      return;
    }

  /* read uncompressed size */
  for (int i = 0; i < 8; i++)
    {
      unsigned char b = this->header[LZMA_PROPS_SIZE + i];
      if (b != 0xFF)
        {
          this->thereIsSize = true;
        }
      this->unpackSize += (UInt64)b << (i * 8);
    }
  
  /* Decode LZMA properties and allocate memory */
  LzmaDec_Construct(&(this->state));
  int res = LzmaDec_Allocate(&(this->state), this->header, LZMA_PROPS_SIZE, &g_Alloc);
  if (res != SZ_OK)
    {
      lasterr = res;
      return;
    }

  LzmaDec_Init(&(this->state));
  initializedOk = true;
}

/* ===========================================================================
 *  duplicates a lot of code in check_header, but we don't want
 *  to read "too much" from the stream in the check_header case.
 *  Here, we don't care because we know compress will call this
 *  method with a peek'ed buffer. We also do not want to modify
 *  the member variables 'header' and 'state'.
 */
bool
compress_lzma::is_lzma (void * buffer, size_t len)
{
  unsigned char local_header[LZMA_PROPS_SIZE + 8];
  CLzmaDec local_state;
  UInt64   local_unpackSize    = 0;
  bool     local_thereIsSize   = false;

  /* read header */
  if (len < LZMA_PROPS_SIZE + 8)
    {
      return false;
    }
  memcpy((void*)local_header, buffer, sizeof(local_header));

  /* read uncompressed size */
  for (int i = 0; i < 8; i++)
    {
      unsigned char b = local_header[LZMA_PROPS_SIZE + i];
      if (b != 0xFF)
        {
          local_thereIsSize = true;
        }
      local_unpackSize += (UInt64)b << (i * 8);
    }

  /* decode header */
  if (LzmaProps_Decode(&(local_state.prop), local_header, LZMA_PROPS_SIZE) != SZ_OK)
    {
      return false;
    }

  return true;
}

