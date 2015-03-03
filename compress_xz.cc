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
 * Portions of bid_xz() and bid_lzma() adapted from the libarchive
 * archive_read_support_compression_xz.c functions xz_bidder_bid()
 * and lzma_bidder_bid(), which are under a BSD license (reproduced
 * below).
 */

#include "compress_xz.h"
#include "LogSingleton.h"

#include <stdexcept>
using namespace std;
#include <errno.h>
#include <memory.h>
#include <malloc.h>

static inline uint32_t
le32dec(const void *pp)
{
  unsigned char const *p = (unsigned char const *)pp;
  return ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
}

static inline uint64_t
le64dec(const void *pp)
{
  unsigned char const *p = (unsigned char const *)pp;
  return (((uint64_t)le32dec(p + 4) << 32) | le32dec(p));
}

/*
 * Predicate: the stream is open for read.
 */
compress_xz::compress_xz (io_stream * parent)
:
  original(NULL),
  owns_original(true),
  peeklen(0),
  lasterr(0),
  compression_type (COMPRESSION_UNKNOWN)
{
  unsigned char * out_block = NULL;
  unsigned char * in_block = NULL;

  /* read only */
  if (!parent || parent->error())
    {
      lasterr = EBADF;
      return;
    }
  original = parent;

  state = (struct private_data *)calloc(sizeof(*state), 1);
  out_block = (unsigned char *)malloc(out_block_size);
  in_block = (unsigned char *)malloc(in_block_size);
  if (state == NULL || out_block == NULL || in_block == NULL)
    {
      free(out_block);
      free(in_block);
      free(state);
      lasterr = ENOMEM;
      return;
    }

  memset(&(state->stream), 0x00, sizeof(state->stream));
  state->out_block_size = out_block_size;
  state->out_block = out_block;
  state->in_block_size = in_block_size;
  state->in_block = in_block;
  state->out_p = out_block;
  state->stream.avail_in = 0;
  state->stream.next_out = state->out_block;
  state->stream.avail_out = state->out_block_size;

  init_decoder ();
}

ssize_t
compress_xz::read (void *buffer, size_t len)
{
  if (   compression_type != COMPRESSION_XZ
      && compression_type != COMPRESSION_LZMA)
    {
      return -1;
    }

  /* there is no recovery from a busted stream */
  if (this->lasterr)
    {
      return -1;
    }
  if (len == 0)
    {
      return 0;
    }

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

  if (state->out_p < state->out_block + state->out_pos)
  /* out_p - out_block < out_pos, but avoid sign/unsigned warning */
    {
      ssize_t tmplen = std::min ((size_t)(state->out_block + state->out_pos - state->out_p), len);
      memcpy (buffer, state->out_p, tmplen);
      state->out_p += tmplen;
      ssize_t tmpread = read (&((char *) buffer)[tmplen], len - tmplen);
      if (tmpread >= 0)
        return tmpread + tmplen;
      else
        return tmpread;
    }

  size_t lenRemaining = len;
  unsigned char * bufp = (unsigned char *)buffer;
  size_t avail_in = 0;
  size_t avail_out = 0;
  size_t decompressed = 0;
  size_t consumed = 0;
  /* if we made it here, any existing uncompressed data in out_block
   * has been consumed, so reset out_p and out_pos
   */
  state->out_p = state->out_block;
  state->out_pos = 0;
  do
    {
      if (state->in_pos == state->in_size)
        {
	  /* no compressed data ready; read some more */
          state->in_size = (size_t) this->original->read(state->in_block, state->in_block_size);
          state->in_pos = 0;
        }

      avail_in = state->in_size - state->in_pos; /* will be 0 if EOF */
      avail_out = state->out_block_size - state->out_pos;

      state->stream.next_out = state->out_block + state->out_pos;
      state->stream.avail_out = avail_out;
      state->stream.next_in = state->in_block + state->in_pos;
      state->stream.avail_in = avail_in;

      lzma_ret res = lzma_code (&(state->stream),
                                (state->stream.avail_in == 0) ? LZMA_FINISH : LZMA_RUN);

      consumed = avail_in - state->stream.avail_in;
      decompressed = avail_out - state->stream.avail_out;

      state->in_pos += consumed;
      state->out_pos += decompressed;

      ssize_t tmplen = std::min (decompressed, lenRemaining);
      memcpy (bufp, state->out_p, tmplen);
      state->out_p += tmplen;
      bufp += tmplen;
      lenRemaining -= tmplen;
      state->total_out += decompressed;
      state->total_in += consumed;

      switch (res)
        {
          case LZMA_STREAM_END: /* Found end of stream. */
            state->eof = 1;
            /* FALL THROUGH */
          case LZMA_OK: /* Decompressor made some progress. */
            break;
          case LZMA_MEM_ERROR:
            LogPlainPrintf ("Lzma library error: Cannot allocate memory\n");
            this->lasterr = ENOMEM;
            return -1;
          case LZMA_MEMLIMIT_ERROR:
            LogPlainPrintf ("Lzma library error: Out of memory\n");
            this->lasterr = ENOMEM;
            return -1;
          case LZMA_FORMAT_ERROR:
            LogPlainPrintf ("Lzma library error: format not recognized\n");
            this->lasterr = EINVAL;
            return -1;
          case LZMA_OPTIONS_ERROR:
            LogPlainPrintf ("Lzma library error: Invalid options\n");
            this->lasterr = EINVAL;
            return -1;
          case LZMA_DATA_ERROR:
            LogPlainPrintf ("Lzma library error: Corrupted input data\n");
            this->lasterr = EINVAL;
            return -1;
          case LZMA_BUF_ERROR:
            LogPlainPrintf ("Lzma library error: No progress is possible\n");
            this->lasterr = EINVAL;
            return -1;
          case LZMA_PROG_ERROR:
            LogPlainPrintf ("Lzma library error: Internal error\n");
            this->lasterr = EINVAL;
            return -1;
          default:
            LogPlainPrintf ("Lzma decompression failed:  Unknown error %d\n", res);
            this->lasterr = EINVAL;
            return -1;
        }
    }
  while (lenRemaining != 0 && !state->eof);

  return (len - lenRemaining);
}

ssize_t
compress_xz::write (const void *buffer, size_t len)
{
  throw new logic_error("compress_xz::write is not implemented");
}

ssize_t
compress_xz::peek (void *buffer, size_t len)
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
compress_xz::tell ()
{
  throw new logic_error("compress_xz::tell is not implemented");
}

int
compress_xz::seek (long where, io_stream_seek_t whence)
{
  throw new logic_error("compress_xz::seek is not implemented");
}

int
compress_xz::error ()
{
  return lasterr;
}

int
compress_xz::set_mtime (time_t mtime)
{
  if (original)
    return original->set_mtime (mtime);
  return 1;
}

time_t
compress_xz::get_mtime ()
{
  if (original)
    return original->get_mtime ();
  return 0;
}

mode_t
compress_xz::get_mode ()
{
  if (original)
    return original->get_mode ();
  return 0;
}

void
compress_xz::release_original ()
{
  owns_original = false;
}

void
compress_xz::destroy ()
{
  if (state)
    {
      if (   compression_type == COMPRESSION_XZ
          || compression_type == COMPRESSION_LZMA)
      {
        lzma_end(&(state->stream));
      }

      if (state->out_block)
        {
          free (state->out_block);
          state->out_block = NULL;
        }

      if (state->in_block)
        {
          free (state->in_block);
          state->in_block = NULL;
        }

      free(state);
      state = NULL;

      compression_type = COMPRESSION_UNKNOWN;
    }

  if (original && owns_original)
    delete original;
}

compress_xz::~compress_xz ()
{
  destroy ();
}

/* ===========================================================================
 *  Check the header of a lzma_stream opened for reading, and initialize
 *  the appropriate decoder (xz or lzma).
 *  IN assertion:
 *     the stream has already been created sucessfully
 *     this method is called only once per stream
 *  OUT assertion - success:
 *     compression_type is set to COMPRESSION_XZ or COMPRESSION_LZMA
 *     state->stream is initialized with the appropriate decoder
 *     lzma: the first 14 bytes of the stream are read (+ whatever
 *         the decoder itself consumes on initialization)
 *     xz: the first 6 bytes of the stram are read (+ whatever the
 *         decoder itself consumes on initialization)
 *     last_error is zero
 *  OUT assertion - error:
 *     last_error is non-zero
 */
void
compress_xz::init_decoder (void)
{
  unsigned char buf[14];
  int ret;
  this->compression_type = COMPRESSION_UNKNOWN;

  /* read properties */
  if (this->original->peek (buf, 6) != 6)
    {
      this->lasterr = (errno ? errno : EIO);
      return;
    }

  if (bid_xz ((void *)buf, 6) > 0)
    {
      this->compression_type = COMPRESSION_XZ;
    }
  else
    {
      if (this->original->peek (buf + 6, 8) != 8)
        {
          this->lasterr = (errno ? errno : EIO);
          return;
        }
      if (bid_lzma ((void *)buf, 14) > 0)
        {
	  this->compression_type = COMPRESSION_LZMA;
	}
    }

  switch (compression_type)
    {
      case COMPRESSION_XZ:
	ret = lzma_stream_decoder (&(state->stream),
                                   (1U << 30),/* memlimit */
                                   LZMA_CONCATENATED);
        break;
      case COMPRESSION_LZMA:
	ret = lzma_alone_decoder (&(state->stream),
                                  (1U << 30));/* memlimit */
        break;
      default:
	this->lasterr = EINVAL;
	return;
    }

  switch (ret)
    {
      case LZMA_OK:
        break;
      case LZMA_MEM_ERROR:
        this->lasterr = ENOMEM;
	break;
      case LZMA_OPTIONS_ERROR:
        this->lasterr = EINVAL;
	break;
      default:
	this->lasterr = EINVAL;
        break;
    }
}

bool
compress_xz::is_xz_or_lzma (void * buffer, size_t len)
{
  int bits_checked_xz;
  int bits_checked_lzma;

  bits_checked_xz = bid_xz (buffer, len);
  if (bits_checked_xz)
    return true;

  bits_checked_lzma = bid_lzma (buffer, len);
  if (bits_checked_lzma)
    return true;

  return false;
}

/*-
 * Portions of bid_xz() and bid_lzma() have been adapted from the
 * libarchive archive_read_support_compression_xz.c functions
 * xz_bidder_bid() and lzma_bidder_bid(), which were released under
 * the 2-clause (simplified) BSD license, reproduced below.
 *
 * (modifications for setup.exe) Copyright (c) 2010 Charles Wilson
 * Copyright (c) 2009 Michihiro NAKAJIMA
 * Copyright (c) 2003-2008 Tim Kientzle and Miklos Vajna
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
int
compress_xz::bid_xz (void * buffer, size_t len)
{
  const unsigned char *buf;
  int bits_checked;

  buf = (const unsigned char *)buffer;
  if (len < 6)
    {
      /* not enough peek'ed data in buf */
      return 0;
    }

  /*
   * Verify Header Magic Bytes : FD 37 7A 58 5A 00
   */
  bits_checked = 0;
  if (buf[0] != 0xFD)
    return 0;
  bits_checked += 8;
  if (buf[1] != 0x37)
    return 0;
  bits_checked += 8;
  if (buf[2] != 0x7A)
    return 0;
  bits_checked += 8;
  if (buf[3] != 0x58)
    return 0;
  bits_checked += 8;
  if (buf[4] != 0x5A)
    return 0;
  bits_checked += 8;
  if (buf[5] != 0x00)
    return 0;
  bits_checked += 8;

  LogBabblePrintf ("compress_xz::bid_xz: success: %d\n", bits_checked);
  return (bits_checked);
}

int
compress_xz::bid_lzma (void * buffer, size_t len)
{
  const unsigned char *buf;
  uint32_t dicsize;
  uint64_t uncompressed_size;
  int bits_checked;

  if (len < 14)
    {
      /* not enough peek'ed data in buffer */
      return 0;
    }
  buf = (unsigned char *)buffer;

  /* First byte of raw LZMA stream is commonly 0x5d.
   * The first byte is a special number, which consists of
   * three parameters of LZMA compression, a number of literal
   * context bits(which is from 0 to 8, default is 3), a number
   * of literal pos bits(which is from 0 to 4, default is 0),
   * a number of pos bits(which is from 0 to 4, default is 2).
   * The first byte is made by
   * (pos bits * 5 + literal pos bit) * 9 + * literal contest bit,
   * and so the default value in this field is
   * (2 * 5 + 0) * 9 + 3 = 0x5d.
   * lzma of LZMA SDK has options to change those parameters.
   * It means a range of this field is from 0 to 224. And lzma of
   * XZ Utils with option -e records 0x5e in this field. */
  /* NOTE: If this checking of the first byte increases false
   * recognition, we should allow only 0x5d and 0x5e for the first
   * byte of LZMA stream. */
  bits_checked = 0;
  if (buf[0] > (4 * 5 + 4) * 9 + 8)
    return 0;
  /* Most likely value in the first byte of LZMA stream. */
  if (buf[0] == 0x5d || buf[0] == 0x5e)
    bits_checked += 8;

  /* Sixth through fourteenth bytes are uncompressed size,
   * stored in little-endian order. `-1' means uncompressed
   * size is unknown and lzma of XZ Utils always records `-1'
   * in this field. */
  uncompressed_size = le64dec(buf+5);
  if (uncompressed_size == (uint64_t)(-1))
    bits_checked += 64;

  /* Second through fifth bytes are dictionary size, stored in
   * little-endian order. The minimum dictionary size is
   * 1 << 12(4KiB) which the lzma of LZMA SDK uses with option
   * -d12 and the maxinam dictionary size is 1 << 27(128MiB)
   * which the one uses with option -d27.
   * NOTE: A comment of LZMA SDK source code says this dictionary
   * range is from 1 << 12 to 1 << 30. */
  dicsize = le32dec(buf+1);
  switch (dicsize)
    {
      case 0x00001000:/* lzma of LZMA SDK option -d12. */
      case 0x00002000:/* lzma of LZMA SDK option -d13. */
      case 0x00004000:/* lzma of LZMA SDK option -d14. */
      case 0x00008000:/* lzma of LZMA SDK option -d15. */
      case 0x00010000:/* lzma of XZ Utils option -0 and -1.
                       * lzma of LZMA SDK option -d16. */
      case 0x00020000:/* lzma of LZMA SDK option -d17. */
      case 0x00040000:/* lzma of LZMA SDK option -d18. */
      case 0x00080000:/* lzma of XZ Utils option -2.
                       * lzma of LZMA SDK option -d19. */
      case 0x00100000:/* lzma of XZ Utils option -3.
                       * lzma of LZMA SDK option -d20. */
      case 0x00200000:/* lzma of XZ Utils option -4.
                       * lzma of LZMA SDK option -d21. */
      case 0x00400000:/* lzma of XZ Utils option -5.
                       * lzma of LZMA SDK option -d22. */
      case 0x00800000:/* lzma of XZ Utils option -6.
                       * lzma of LZMA SDK option -d23. */
      case 0x01000000:/* lzma of XZ Utils option -7.
                       * lzma of LZMA SDK option -d24. */
      case 0x02000000:/* lzma of XZ Utils option -8.
                       * lzma of LZMA SDK option -d25. */
      case 0x04000000:/* lzma of XZ Utils option -9.
                       * lzma of LZMA SDK option -d26. */
      case 0x08000000:/* lzma of LZMA SDK option -d27. */
        bits_checked += 32;
        break;
      default:
        /* If a memory usage for encoding was not enough on
         * the platform where LZMA stream was made, lzma of
         * XZ Utils automatically decreased the dictionary
         * size to enough memory for encoding by 1Mi bytes
         * (1 << 20).*/
        if (dicsize <= 0x03F00000 && dicsize >= 0x00300000
            && (dicsize & ((1 << 20)-1)) == 0
            && bits_checked == 8 + 64)
          {
            bits_checked += 32;
            break;
          }
        /* Otherwise dictionary size is unlikely. But it is
         * possible that someone makes lzma stream with
         * liblzma/LZMA SDK in one's dictionary size. */
        return 0;
    }

  /* TODO: The above test is still very weak.  It would be
   * good to do better. */
  LogBabblePrintf ("compress_xz::bid_lzma: success: %d\n", bits_checked);
  return (bits_checked);
}


