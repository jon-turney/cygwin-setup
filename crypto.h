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
 * Written by Dave Korn <dave.korn.cygwin@gmail.com>
 *
 */

#ifndef SETUP_CRYPTO_H
#define SETUP_CRYPTO_H

/* This module uses libgcrypt functionality to verify signatures
 * on downloaded setup.ini or setup.bz2 files.
 */

/* for HWND */
#include "win32.h" 
class io_stream;

/*  This is currently the only public API exported by the module; it
  takes the contents of setup.ini or setup.bz2 in one (memory-based,
  for preference) io_stream, and the contents of the related signature
  file in another.  It is called from ini.cc/do_remote_ini() and returns
  true if the signature verified OK; if it returns false, you MUST NOT
  use the failed ini file - doubly so if it's a compressed stream!  */
extern bool verify_ini_file_sig (io_stream *ini_file, io_stream *ini_sig_file, HWND owner);

/*
5.2.2.  Version 3 Signature Packet Format

   The body of a version 3 Signature Packet contains:

     - One-octet version number (3).

     - One-octet length of following hashed material.  MUST be 5.

         - One-octet signature type.

         - Four-octet creation time.

     - Eight-octet Key ID of signer.

     - One-octet public-key algorithm.

     - One-octet hash algorithm.

     - Two-octet field holding left 16 bits of signed hash value.

     - One or more multiprecision integers comprising the signature.
       This portion is algorithm specific, as described below.

   The concatenation of the data to be signed, the signature type, and
   creation time from the Signature packet (5 additional octets) is
   hashed.  The resulting hash value is used in the signature algorithm.
   The high 16 bits (first two octets) of the hash are included in the
   Signature packet to provide a quick test to reject some invalid
   signatures.

   Algorithm-Specific Fields for RSA signatures:

     - multiprecision integer (MPI) of RSA signature value m**d mod n.

   Algorithm-Specific Fields for DSA signatures:

     - MPI of DSA value r.

     - MPI of DSA value s.

   The signature calculation is based on a hash of the signed data, as
   described above.  The details of the calculation are different for
   DSA signatures than for RSA signatures.

   With RSA signatures, the hash value is encoded using PKCS#1 encoding
   type EMSA-PKCS1-v1_5 as described in Section 9.2 of RFC 3447.  This
   requires inserting the hash value as an octet string into an ASN.1
   structure.  The object identifier for the type of hash being used is
   included in the structure.  The hexadecimal representations for the
   currently defined hash algorithms are as follows:

     - MD5:        0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x02, 0x05

     - RIPEMD-160: 0x2B, 0x24, 0x03, 0x02, 0x01

     - SHA-1:      0x2B, 0x0E, 0x03, 0x02, 0x1A

     - SHA224:     0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04

     - SHA256:     0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01

     - SHA384:     0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02

     - SHA512:     0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03

   The ASN.1 Object Identifiers (OIDs) are as follows:

     - MD5:        1.2.840.113549.2.5

     - RIPEMD-160: 1.3.36.3.2.1

     - SHA-1:      1.3.14.3.2.26

     - SHA224:     2.16.840.1.101.3.4.2.4

     - SHA256:     2.16.840.1.101.3.4.2.1

     - SHA384:     2.16.840.1.101.3.4.2.2

     - SHA512:     2.16.840.1.101.3.4.2.3

   The full hash prefixes for these are as follows:

       MD5:        0x30, 0x20, 0x30, 0x0C, 0x06, 0x08, 0x2A, 0x86,
                   0x48, 0x86, 0xF7, 0x0D, 0x02, 0x05, 0x05, 0x00,
                   0x04, 0x10

       RIPEMD-160: 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B, 0x24,
                   0x03, 0x02, 0x01, 0x05, 0x00, 0x04, 0x14

       SHA-1:      0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0E,
                   0x03, 0x02, 0x1A, 0x05, 0x00, 0x04, 0x14

       SHA224:     0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
                   0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04, 0x05,
                   0x00, 0x04, 0x1C

       SHA256:     0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
                   0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
                   0x00, 0x04, 0x20

       SHA384:     0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
                   0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05,
                   0x00, 0x04, 0x30

       SHA512:     0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
                   0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
                   0x00, 0x04, 0x40

   DSA signatures MUST use hashes that are equal in size to the number
   of bits of q, the group generated by the DSA key's generator value.

   If the output size of the chosen hash is larger than the number of
   bits of q, the hash result is truncated to fit by taking the number
   of leftmost bits equal to the number of bits of q.  This (possibly
   truncated) hash function result is treated as a number and used
   directly in the DSA signature algorithm.


5.2.3.  Version 4 Signature Packet Format

   The body of a version 4 Signature packet contains:

     - One-octet version number (4).

     - One-octet signature type.

     - One-octet public-key algorithm.

     - One-octet hash algorithm.

     - Two-octet scalar octet count for following hashed subpacket data.
       Note that this is the length in octets of all of the hashed
       subpackets; a pointer incremented by this number will skip over
       the hashed subpackets.

     - Hashed subpacket data set (zero or more subpackets).

     - Two-octet scalar octet count for the following unhashed subpacket
       data.  Note that this is the length in octets of all of the
       unhashed subpackets; a pointer incremented by this number will
       skip over the unhashed subpackets.

     - Unhashed subpacket data set (zero or more subpackets).

     - Two-octet field holding the left 16 bits of the signed hash
       value.

     - One or more multiprecision integers comprising the signature.
       This portion is algorithm specific, as described above.

   The concatenation of the data being signed and the signature data
   from the version number through the hashed subpacket data (inclusive)
   is hashed.  The resulting hash value is what is signed.  The left 16
   bits of the hash are included in the Signature packet to provide a
   quick test to reject some invalid signatures.

   There are two fields consisting of Signature subpackets.  The first
   field is hashed with the rest of the signature data, while the second
   is unhashed.  The second set of subpackets is not cryptographically
   protected by the signature and should include only advisory
   information.

   The algorithms for converting the hash function result to a signature
   are described in a section below.


5.2.4.  Computing Signatures

   All signatures are formed by producing a hash over the signature
   data, and then using the resulting hash in the signature algorithm.

   For binary document signatures (type 0x00), the document data is
   hashed directly.  For text document signatures (type 0x01), the
   document is canonicalized by converting line endings to <CR><LF>,
   and the resulting data is hashed.

   When a signature is made over a key, the hash data starts with the
   octet 0x99, [ ... ]

   A certification signature (type 0x10 through 0x13) [ ... ]

   When a signature is made over a Signature packet (type 0x50), [ ... ]

   Once the data body is hashed, then a trailer is hashed.  A V3
   signature hashes five octets of the packet body, starting from the
   signature type field.  This data is the signature type, followed by
   the four-octet signature time.  A V4 signature hashes the packet body
   starting from its first field, the version number, through the end
   of the hashed subpacket data.  Thus, the fields hashed are the
   signature version, the signature type, the public-key algorithm, the
   hash algorithm, the hashed subpacket length, and the hashed
   subpacket body.

   V4 signatures also hash in a final trailer of six octets: the
   version of the Signature packet, i.e., 0x04; 0xFF; and a four-octet,
   big-endian number that is the length of the hashed data from the
   Signature packet (note that this number does not include these final
   six octets).

   After all this has been hashed in a single hash context, the
   resulting hash field is used in the signature algorithm and placed
   at the end of the Signature packet.

*/

#define RFC4880_SIGV3_HASHED_SIZE       5
#define RFC4880_SIGV4_HASHED_OVERHEAD   6


/*
5.5.  Key Material Packet

   A key material packet contains all the information about a public or
   private key.  There are four variants of this packet type, and two
   major versions.  Consequently, this section is complex.

5.5.1.  Key Packet Variants

5.5.1.1.  Public-Key Packet (Tag 6)

   A Public-Key packet starts a series of packets that forms an OpenPGP
   key (sometimes called an OpenPGP certificate).

5.5.1.2.  Public-Subkey Packet (Tag 14)
5.5.1.3.  Secret-Key Packet (Tag 5)
5.5.1.4.  Secret-Subkey Packet (Tag 7)

5.5.2.  Public-Key Packet Formats

   The version 4 format is similar to the version 3 format except for
   the absence of a validity period.  This has been moved to the
   Signature packet.  In addition, fingerprints of version 4 keys are
   calculated differently from version 3 keys, as described in the
   section "Enhanced Key Formats".

   A version 4 packet contains:

     - A one-octet version number (4).

     - A four-octet number denoting the time that the key was created.

     - A one-octet number denoting the public-key algorithm of this key.

     - A series of multiprecision integers comprising the key material.
       This algorithm-specific portion is:

       Algorithm-Specific Fields for RSA public keys:

         - multiprecision integer (MPI) of RSA public modulus n;

         - MPI of RSA public encryption exponent e.

       Algorithm-Specific Fields for DSA public keys:

         - MPI of DSA prime p;

         - MPI of DSA group order q (q is a prime divisor of p-1);

         - MPI of DSA group generator g;

         - MPI of DSA public-key value y (= g**x mod p where x
           is secret).

       Algorithm-Specific Fields for Elgamal public keys:

         - MPI of Elgamal prime p;

         - MPI of Elgamal group generator g;

         - MPI of Elgamal public key value y (= g**x mod p where x
           is secret).

*/

// Big enough to dump the coefficients of a DSA
// signing key of any reasonable size in ASCII
// s-expr representation.
#define GPG_KEY_SEXPR_BUF_SIZE  (8192)

// As long as you respect this maximum coefficient size.
#define GPG_KEY_MAX_COEFF_SIZE  (8192)

#endif /* SETUP_CRYPTO_H */
