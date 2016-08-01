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

#ifndef SETUP_GPG_PACKET_H
#define SETUP_GPG_PACKET_H


/*

4.3.  Packet Tags

   The packet tag denotes what type of packet the body holds.  Note that
   old format headers can only have tags less than 16, whereas new
   format headers can have tags as great as 63.  The defined tags (in
   decimal) are as follows:

       0        -- Reserved - a packet tag MUST NOT have this value
       1        -- Public-Key Encrypted Session Key Packet
       2        -- Signature Packet
       3        -- Symmetric-Key Encrypted Session Key Packet
       4        -- One-Pass Signature Packet
       5        -- Secret-Key Packet
       6        -- Public-Key Packet
       7        -- Secret-Subkey Packet
       8        -- Compressed Data Packet
       9        -- Symmetrically Encrypted Data Packet
       10       -- Marker Packet
       11       -- Literal Data Packet
       12       -- Trust Packet
       13       -- User ID Packet
       14       -- Public-Subkey Packet
       17       -- User Attribute Packet
       18       -- Sym. Encrypted and Integrity Protected Data Packet
       19       -- Modification Detection Code Packet
       60 to 63 -- Private or Experimental Values


*/

#define RFC4880_PT_SIGNATURE  2
#define RFC4880_PT_PUBLIC_KEY 6


/*

9.1.  Public-Key Algorithms

      ID           Algorithm
      --           ---------
      1          - RSA (Encrypt or Sign) [HAC]
      2          - RSA Encrypt-Only [HAC]
      3          - RSA Sign-Only [HAC]
      16         - Elgamal (Encrypt-Only) [ELGAMAL] [HAC]
      17         - DSA (Digital Signature Algorithm) [FIPS186] [HAC]
      18         - Reserved for Elliptic Curve
      19         - Reserved for ECDSA
      20         - Reserved (formerly Elgamal Encrypt or Sign)
      21         - Reserved for Diffie-Hellman (X9.42,
                   as defined for IETF-S/MIME)
      100 to 110 - Private/Experimental algorithm

   Implementations MUST implement DSA for signatures, and Elgamal for
   encryption.  Implementations SHOULD implement RSA keys (1).  RSA
   Encrypt-Only (2) and RSA Sign-Only are deprecated and SHOULD NOT be
   generated, but may be interpreted.  See Section 13.5.  See Section
   13.8 for notes on Elliptic Curve (18), ECDSA (19), Elgamal Encrypt or
   Sign (20), and X9.42 (21).  Implementations MAY implement any other
   algorithm.

*/

#define RFC4880_PK_RSA        1
#define RFC4880_PK_RSA_EO     2
#define RFC4880_PK_RSA_SO     3
#define RFC4880_PK_ELGAMAL    16
#define RFC4880_PK_DSA        17


/*
5.2.1.  Signature Types

   There are a number of possible meanings for a signature, which are
   indicated in a signature type octet in any given signature.  Please
   note that the vagueness of these meanings is not a flaw, but a
   feature of the system.  Because OpenPGP places final authority for
   validity upon the receiver of a signature, it may be that one
   signer's casual act might be more rigorous than some other
   authority's positive act.  See Section 5.2.4, "Computing Signatures",
   for detailed information on how to compute and verify signatures of
   each type.

   These meanings are as follows:

   0x00: Signature of a binary document.
       This means the signer owns it, created it, or certifies that it
       has not been modified.

   0x01: Signature of a canonical text document.
       This means the signer owns it, created it, or certifies that it
       has not been modified.  The signature is calculated over the text
       data with its line endings converted to <CR><LF>.

   0x02: Standalone signature.
   0x10: Generic certification of a User ID and Public-Key packet.
   0x11: Persona certification of a User ID and Public-Key packet.
   0x12: Casual certification of a User ID and Public-Key packet.
   0x13: Positive certification of a User ID and Public-Key packet.
   0x18: Subkey Binding Signature
   0x19: Primary Key Binding Signature
   0x1F: Signature directly on a key
   0x20: Key revocation signature
   0x28: Subkey revocation signature
   0x30: Certification revocation signature
   0x40: Timestamp signature.
   0x50: Third-Party Confirmation signature.

*/
#define RFC4880_ST_BINARY     0
#define RFC4880_ST_CANONTEXT  1


/*
9.4.  Hash Algorithms

      ID           Algorithm                             Text Name
      --           ---------                             ---------
      1          - MD5 [HAC]                             "MD5"
      2          - SHA-1 [FIPS180]                       "SHA1"
      3          - RIPE-MD/160 [HAC]                     "RIPEMD160"
      4          - Reserved
      5          - Reserved
      6          - Reserved
      7          - Reserved
      8          - SHA256 [FIPS180]                      "SHA256"
      9          - SHA384 [FIPS180]                      "SHA384"
      10         - SHA512 [FIPS180]                      "SHA512"
      11         - SHA224 [FIPS180]                      "SHA224"
      100 to 110 - Private/Experimental algorithm

   Implementations MUST implement SHA-1.  Implementations MAY implement
   other algorithms.  MD5 is deprecated.
*/

#define RFC4880_HC_MD5        1
#define RFC4880_HC_SHA1       2
#define RFC4880_HC_RIPEMD160  3
#define RFC4880_HC_SHA256     8
#define RFC4880_HC_SHA384     9
#define RFC4880_HC_SHA512     10
#define RFC4880_HC_SHA224     11


// This enum is returned by the callback function that is
// invoked by the packet walker for every packet walked;
// it tells it to continue or go home early.
enum pkt_cb_resp
{
  pktCONTINUE, 
  pktHALT
};

// Forward declaration of context data struct.
struct packet_walker;

// The type of callback function that can be called for every
// packet walked.
typedef enum pkt_cb_resp (*packet_walk_cb)
	(struct packet_walker *wlk, unsigned char tag, size_t packetsize,
	size_t hdrpos);

// This struct is used to wrap the context data for a packet walk.
struct packet_walker
{
  io_stream *pfile;
  packet_walk_cb func;
  HWND owner;
  void *userdata;
  size_t startpos;
  size_t size_to_walk;
  bool is_subpackets;
};

/* 

3.  Data Element Formats

   This section describes the data elements used by OpenPGP.

3.1.  Scalar Numbers

   Scalar numbers are unsigned and are always stored in big-endian
   format.  Using n[k] to refer to the kth octet being interpreted, the
   value of a two-octet scalar is ((n[0] << 8) + n[1]).  The value of a
   four-octet scalar is ((n[0] << 24) + (n[1] << 16) + (n[2] << 8) +
   n[3]).

*/

/*  Extract a byte/char from file.  Returns EOF if none left. */
static inline int
pkt_getch (io_stream *file)
{
  unsigned char ch;
  if (file->read (&ch, 1) != 1)
    return EOF;
  return ch;
}

/*  Extract a 16-bit BE int from file.  Returns EOF if none left. */
static inline long
pkt_getword (io_stream *file)
{
  unsigned char ch[2];
  if (file->read (&ch, 2) != 2)
    return EOF;
  return (ch[0] << 8) | ch[1];
}

/*  Extract a 32-bit BE int from file.  Returns EOF if none left.
  Determining the difference between EOF and 0xffffffff is left
  as an exercise for the caller - in the contexts where we need
  a dword (packet len, timestamp, signer id), we wouldn't expect
  to find ~0 anyway and so may safely leave it as a false positive.
  Note that this would cause problems with setup.ini files signed
  in the last second before the epoch rolls over.  Workaround: WDDTT.  */
static inline long
pkt_getdword (io_stream *file)
{
  unsigned char ch[4];
  if (file->read (&ch, 4) != 4)
    return EOF;
  return (ch[0] << 24) | (ch[1] << 16) | (ch[2] << 8) | ch[3];
}

/*  Extract an RFC4880 variable-length length field from file.
   Returns EOF if none left or negative if invalid format. */
extern long pkt_getlen (io_stream *file);

/*  Extract an RFC4880 MPI field from file.
   Returns EOF if none left or negative if invalid format. */
extern int pkt_get_mpi (gcry_mpi_t *mpiptr, io_stream *file);

/*  Converts from RFC4880 hash codes (9.4 above) to the hash 
   algorithm constants used in libgcrypt and the rest of the code.  */
extern char pkt_convert_hashcode (char rfc_hash);

/*  Two functions for walking the (sub)packets found within a
   seleected region of an io_stream, calling a hook for each one.  */
extern void *pkt_walk_packets (io_stream *packet_file, packet_walk_cb func, 
	HWND owner, size_t startpos, size_t size_to_walk, void *userdata);

extern void *pkt_walk_subpackets (io_stream *packet_file, packet_walk_cb func,
	HWND owner, size_t startpos, size_t size_to_walk, void *userdata);



#endif /* SETUP_GPG_PACKET_H */
