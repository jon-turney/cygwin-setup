/*
 * Copyright (c) 2018, Cygwin
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#include "compress_zstd.h"

#include <stdexcept>

#include <errno.h>
#include <memory.h>
#include <malloc.h>

/*
 * Predicate: the stream is open for read.
 */
compress_zstd::compress_zstd (io_stream * parent)
:
  original(NULL),
  owns_original(true),
  lasterr(0)
{
  /* read only */
  if (!parent || parent->error())
    {
      lasterr = EBADF;
      return;
    }
  original = parent;

  state = (struct private_data *)calloc(sizeof(*state), 1);
  if (state == NULL)
    {
      free(state);
      lasterr = ENOMEM;
      return;
    }

  state->stream = ZSTD_createDStream();
  if (state->stream == NULL)
    {
      free(state);
      lasterr = ENOMEM;
      return;
    }
  ZSTD_initDStream(state->stream);
  state->out_block.size = state->out_block.pos  = state->out_pos = state->out_bsz = ZSTD_DStreamOutSize();
  state->out_block.dst  = (unsigned char *)malloc(state->out_bsz);
  if (state->out_block.dst == NULL)
    {
      free(state->out_block.dst);
      free(state);
      lasterr = ENOMEM;
      return;
    }
  state->in_block.size = state->in_block.pos  = state->in_bsz = ZSTD_DStreamInSize();
  state->in_block.src  = (unsigned char *)malloc(state->in_bsz);
  state->in_pos  = 0;
  if (state->in_block.src == NULL)
    {
      free(state->out_block.dst);
      free((void*)state->in_block.src);
      free(state);
      lasterr = ENOMEM;
      return;
    }
}

ssize_t
compress_zstd::read (void *buffer, size_t len)
{
  /* there is no recovery from a busted stream */
  if (this->lasterr)
    {
      return -1;
    }
  if (len == 0)
    {
      return 0;
    }

  size_t lenRemaining = len;
  size_t lenBuffered  = 0;
  do
    {
      if (state->in_block.size > 0 && state->in_block.pos >= state->in_block.size)
        {
	  /* no compressed data ready; read some more input */
	  state->in_block.size = state->in_bsz;
	  ssize_t got = this->original->read((void *)state->in_block.src, state->in_bsz);
	  if (got >= 0)
	    {
	      state->in_block.size = got;
	      state->in_block.pos = 0;
	    }
	  else
	    {
	      lasterr = EIO;
	      return -1;
	    }
	  continue;
        }

      if (state->out_pos < state->out_block.pos)
	{
	  /* output buffer has unused data */
	  ssize_t tmplen = std::min (state->out_block.pos - state->out_pos, lenRemaining);
	  memcpy (&((char *)buffer)[lenBuffered], &((char *)state->out_block.dst)[state->out_pos], tmplen);
	  state->out_pos += tmplen;
	  lenBuffered    += tmplen;
	  lenRemaining   -= tmplen;
	  if (state->eof)
	    {
	      break;
	    }
	}
      else
	{
	  if (state->eof)
	    {
	      break;
	    }
	  /* output buffer is empty; decompress more data */
	  state->out_block.size = state->out_bsz;
	  state->out_pos = state->out_block.pos = 0;
	  size_t ret = ZSTD_decompressStream (state->stream, &state->out_block, &state->in_block);
	  if (ZSTD_isError(ret))
	    {
	      // TODO return/print error
	      return -1;
	    }
	  state->eof = (ret == 0);
	}
    }
  while (lenRemaining != 0);

  return (len - lenRemaining);
}

ssize_t
compress_zstd::write (const void *buffer, size_t len)
{
  throw new std::logic_error("compress_zstd::write is not implemented");
}

ssize_t
compress_zstd::peek (void *buffer, size_t len)
{
  /* can only peek 512 bytes */
  if (len > 512)
    return ENOMEM;

  // we only peek at the beginning of a file, so no buffer tearing can happen
  // do a real read first…
  ssize_t got = read (buffer, len);
  if (got >= 0)
    {
      // …then rewind read position for the next read()
      state->out_pos -= got;
    }
  /* error */
  return got;
}

long
compress_zstd::tell ()
{
  throw new std::logic_error("compress_zstd::tell is not implemented");
}

int
compress_zstd::seek (long where, io_stream_seek_t whence)
{
  throw new std::logic_error("compress_zstd::seek is not implemented");
}

int
compress_zstd::error ()
{
  return lasterr;
}

int
compress_zstd::set_mtime (time_t mtime)
{
  if (original)
    return original->set_mtime (mtime);
  return 1;
}

time_t
compress_zstd::get_mtime ()
{
  if (original)
    return original->get_mtime ();
  return 0;
}

mode_t
compress_zstd::get_mode ()
{
  if (original)
    return original->get_mode ();
  return 0;
}

void
compress_zstd::release_original ()
{
  owns_original = false;
}

void
compress_zstd::destroy ()
{
  if (state)
    {
      ZSTD_freeDStream(state->stream);

      if (state->out_block.dst)
        {
          free (state->out_block.dst);
          state->out_block.dst = NULL;
        }

      if (state->in_block.src)
        {
          free ((void*)state->in_block.src);
          state->in_block.src = NULL;
        }

      free(state);
      state = NULL;
    }

  if (original && owns_original)
    delete original;
}

compress_zstd::~compress_zstd ()
{
  destroy ();
}

bool
compress_zstd::is_zstd (void * buffer, size_t len)
{
  return (ZSTD_getFrameContentSize(buffer, len) != ZSTD_CONTENTSIZE_ERROR);
}
