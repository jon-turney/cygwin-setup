/*
 * Copyright (c) 2008, Dave Korn.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 *   This module contains support utilities to assist in reading and
 * parsing RFC4880-compliant OpenPGP format signature and key files,
 * and related constant definitions.
 *
 *
 * Written by Dave Korn <dave.korn.cygwin@gmail.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "win32.h"
#include "io_stream.h"
#include "gcrypt.h"
#include "gpg-packet.h"
#include "msg.h"
#include "LogSingleton.h"
#include "resource.h"

#define CRYPTODEBUGGING         (0)

#if CRYPTODEBUGGING
#define ERRKIND __asm__ __volatile__ (".byte 0xcc"); note
#define MESSAGE LogBabblePrintf
#else  /* !CRYPTODEBUGGING */
#define ERRKIND note
#define MESSAGE while (0) LogBabblePrintf
#endif /* CRYPTODEBUGGING */

#ifndef ARRAYSIZE
#define ARRAYSIZE(_ar) (sizeof (_ar) / sizeof (_ar[0]))
#endif

static const struct { char from; char to; } RFC4880HashCodesToGPGHashCodes[] =
{
  { RFC4880_HC_MD5,       GCRY_MD_MD5    },
  { RFC4880_HC_SHA1,      GCRY_MD_SHA1   },
  { RFC4880_HC_RIPEMD160, GCRY_MD_RMD160 },
  { RFC4880_HC_SHA256,    GCRY_MD_SHA256 },
  { RFC4880_HC_SHA384,    GCRY_MD_SHA384 },
  { RFC4880_HC_SHA512,    GCRY_MD_SHA512 },
  { RFC4880_HC_SHA224,    GCRY_MD_SHA224 }
};

char
pkt_convert_hashcode (char rfc_hash)
{
  for (unsigned int i = 0; i < ARRAYSIZE(RFC4880HashCodesToGPGHashCodes); i++)
    if (RFC4880HashCodesToGPGHashCodes[i].from == rfc_hash)
      return RFC4880HashCodesToGPGHashCodes[i].to;
  return GCRY_MD_NONE;
}

/*
4.2.2.  New Format Packet Lengths

   New format packets have four possible ways of encoding length:

   1. A one-octet Body Length header encodes packet lengths of up to 191
      octets.

   2. A two-octet Body Length header encodes packet lengths of 192 to
      8383 octets.

   3. A five-octet Body Length header encodes packet lengths of up to
      4,294,967,295 (0xFFFFFFFF) octets in length.  (This actually
      encodes a four-octet scalar number.)

   4. When the length of the packet body is not known in advance by the
      issuer, Partial Body Length headers encode a packet of
      indeterminate length, effectively making it a stream.

4.2.2.1.  One-Octet Lengths

   A one-octet Body Length header encodes a length of 0 to 191 octets.
   This type of length header is recognized because the one octet value
   is less than 192.  The body length is equal to:

       bodyLen = 1st_octet;

4.2.2.2.  Two-Octet Lengths

   A two-octet Body Length header encodes a length of 192 to 8383
   octets.  It is recognized because its first octet is in the range 192
   to 223.  The body length is equal to:

       bodyLen = ((1st_octet - 192) << 8) + (2nd_octet) + 192

4.2.2.3.  Five-Octet Lengths

   A five-octet Body Length header consists of a single octet holding
   the value 255, followed by a four-octet scalar.  The body length is
   equal to:

       bodyLen = (2nd_octet << 24) | (3rd_octet << 16) |
                 (4th_octet << 8)  | 5th_octet

   This basic set of one, two, and five-octet lengths is also used
   internally to some packets.

*/
long
pkt_getlen (io_stream *file)
{
  int ch1, ch2;

  ch1 = pkt_getch (file);
  // Obviously these two conditions fold into one, but since
  // one is an error test and the other a range check it's 
  // nice to write them separately and let the compiler take
  // care of it.
  if (ch1 < 0)
    return ch1;
  if (ch1 < 192)
    return ch1;

  if (ch1 == 255)
    return pkt_getdword (file);

  ch2 = pkt_getch (file);
  if (ch2 < 0)
    return ch2;
  if (ch1 < 224)
    return ((ch1 - 192) << 8) + (ch2) + 192;
  return -2;
}

/*
3.2.  Multiprecision Integers

   Multiprecision integers (also called MPIs) are unsigned integers used
   to hold large integers such as the ones used in cryptographic
   calculations.

   An MPI consists of two pieces: a two-octet scalar that is the length
   of the MPI in bits followed by a string of octets that contain the
   actual integer.

   These octets form a big-endian number; a big-endian number can be
   made into an MPI by prefixing it with the appropriate length.

   Examples:

   (all numbers are in hexadecimal)

   The string of octets [00 01 01] forms an MPI with the value 1.  The
   string [00 09 01 FF] forms an MPI with the value of 511.

   Additional rules:

   The size of an MPI is ((MPI.length + 7) / 8) + 2 octets.

   The length field of an MPI describes the length starting from its
   most significant non-zero bit.  Thus, the MPI [00 02 01] is not
   formed correctly.  It should be [00 01 01].

   Unused bits of an MPI MUST be zero.

   Also note that when an MPI is encrypted, the length refers to the
   plaintext MPI.  It may be ill-formed in its ciphertext.

*/
int
pkt_get_mpi (gcry_mpi_t *mpiptr, io_stream *file)
{
   /*  "An MPI consists of two pieces: a two-octet scalar that is the
   length of the MPI in bits followed by a string of octets that contain
   the actual integer."  */

  long nbits = pkt_getword (file);

  if (nbits < 0)
    return nbits;

  size_t nbytes = ((nbits + 7) >> 3);

  unsigned char *tmpbuf = new unsigned char [nbytes];

  if (file->read (tmpbuf, nbytes) != (ssize_t)nbytes)
    return -2;

  gcry_error_t rv = gcry_mpi_scan (mpiptr, GCRYMPI_FMT_USG, tmpbuf, nbytes, 0UL);

  delete[] tmpbuf;

  if (rv != GPG_ERR_NO_ERROR)
    return -3;

  return 0;
}

/*  Walk the packets in an io_stream.  */
static void
walk_packets_1 (struct packet_walker *wlk)
{
  long size_left = wlk->size_to_walk;
  size_t ostartpos = wlk->startpos;
  MESSAGE ("walk $%08x bytes at startpos $%08x\n", wlk->size_to_walk, wlk->startpos);
  while (size_left)
    {
      char packet_type;
      long packet_len;
      enum pkt_cb_resp rv;
      size_t newstartpos;

      wlk->pfile->seek (wlk->startpos, IO_SEEK_SET);

      if (wlk->is_subpackets)
	packet_len = pkt_getlen (wlk->pfile) - 1;
      else
	packet_len = -1;

      int tag = pkt_getch (wlk->pfile);
      MESSAGE ("tag $%02x size $%08x\n", tag, size_left);

      if (!wlk->is_subpackets && ((tag < 0) || !(tag & 0x80)))
	{
	  ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, tag, "illegal tag.");
	  return;
	}

      if (wlk->is_subpackets)
	packet_type = tag;
      else if (tag & 0x40)
	{
	  packet_type = tag & 0x3f;
	  packet_len = pkt_getlen (wlk->pfile);
	}
      else
	{
	  packet_type = (tag >> 2) & 0x0f;
	  switch (tag & 3)
	    {
	      case 0:
	        packet_len = pkt_getch (wlk->pfile);
		break;
	      case 1:
	        packet_len = pkt_getword (wlk->pfile);
		break;
	      case 2:
	        packet_len = pkt_getdword (wlk->pfile);
		break;
	      case 3:
		ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, tag, "illegal old tag.");
		return;
	    }
	}

      MESSAGE ("type $%02x len $%08x pos $%08x\n", packet_type, packet_len,
	wlk->pfile->tell ());
      if (packet_len < 0)
	{
	  ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, packet_len, "invalid packet");
	  return;
	}
      else if (packet_len > (size_left - (wlk->pfile->tell () - (long)wlk->startpos)))
	{
	  ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, packet_len, "malformed packet");
	  return;
	}

      newstartpos = wlk->pfile->tell () + packet_len;
      rv = wlk->func ? wlk->func (wlk, packet_type, packet_len, wlk->startpos)
		      : pktCONTINUE;
      if (rv == pktHALT)
	break;

      wlk->startpos = newstartpos;
      wlk->pfile->seek (wlk->startpos, IO_SEEK_SET);
      size_left = wlk->size_to_walk - (wlk->startpos - ostartpos);
      MESSAGE ("remaining $%08x nextpos $%08x\n", size_left, wlk->startpos);
    }
}

void *
pkt_walk_packets (io_stream *packet_file, packet_walk_cb func, HWND owner,
	size_t startpos, size_t size_to_walk, void *userdata)
{
  struct packet_walker wlk;
  wlk.pfile = packet_file;
  wlk.func = func;
  wlk.owner = owner;
  wlk.userdata = userdata;
  wlk.startpos = startpos;
  wlk.size_to_walk = size_to_walk;
  wlk.is_subpackets = false;
  walk_packets_1 (&wlk);
  return wlk.userdata;
}

void *
pkt_walk_subpackets (io_stream *packet_file, packet_walk_cb func, HWND owner,
	size_t startpos, size_t size_to_walk, void *userdata)
{
  struct packet_walker wlk;
  wlk.pfile = packet_file;
  wlk.func = func;
  wlk.owner = owner;
  wlk.userdata = userdata;
  wlk.startpos = startpos;
  wlk.size_to_walk = size_to_walk;
  wlk.is_subpackets = true;
  walk_packets_1 (&wlk);
  return wlk.userdata;
}

