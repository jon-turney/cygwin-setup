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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include "io_stream.h"
#include "crypto.h"
#include "compress.h"
#include "gcrypt.h"
#include "msg.h"
#include "LogSingleton.h"
#include "resource.h"
#include "getopt++/StringArrayOption.h"
#include "getopt++/BoolOption.h"
#include "KeysSetting.h"
#include "gpg-packet.h"
#include "geturl.h"

#define CRYPTODEBUGGING         (0)

#if CRYPTODEBUGGING
#define ERRKIND __asm__ __volatile__ (".byte 0xcc"); note
#define MESSAGE LogBabblePrintf
#else  /* !CRYPTODEBUGGING */
#define ERRKIND note
#define MESSAGE while (0) LogBabblePrintf
#endif /* CRYPTODEBUGGING */

/*  Command-line options for specifying and controlling extra keys.  */
static StringArrayOption ExtraKeyOption ('K', "pubkey",
			"URL of extra public key file (gpg format)");

static StringArrayOption SexprExtraKeyOption ('S', "sexpr-pubkey",
			"Extra public key in s-expr format");

static BoolOption UntrustedKeysOption (false, 'u', "untrusted-keys",
			"Use untrusted saved extra keys");
static BoolOption KeepUntrustedKeysOption (false, 'U', "keep-untrusted-keys",
			"Use untrusted keys and retain all");

/*  Embedded public half of Cygwin DSA signing key.  */
static const char *cygwin_pubkey_sexpr = 
#include "cyg-pubkey.h"
;

/*  S-expr template for DSA pubkey.  */
static const char *dsa_pubkey_templ = "(public-key (dsa (p %m) (q %m) (g %m) (y %m)))";

/*  S-expr template for DSA signature.  */
static const char *dsa_sig_templ = "(sig-val (dsa (r %m) (s %m)))";

/*  S-expr template for data block to be signed.  */
static const char *data_hash_templ = "(data (flags raw) (value %m))";

/*  User context data for sig packet walk.  */
struct sig_data
{
  /*  MPI values of sig components.  */
  gcry_mpi_t dsa_mpi_r, dsa_mpi_s;

  /*  Hash context.  */
  gcry_md_hd_t md;

  /*  Main data.  */
  io_stream *sign_data;

  /*  Auxiliary data.  */
  int sig_type;
  int pk_alg;
  int hash_alg;

  /*  Converted algo code.  */
  int algo;

  /*  Final status.  */
  bool complete;
};

/*  User context data for key packet walk.  */
struct key_data
{
  std::vector<gcry_sexp_t> keys;
};

/*  Callback hook for walking packets in gpg key file.  Extracts
  the DSA coefficients from any public key packets encountered and 
  converts them into s-expr pubkey format, returning the public
  keys thus found to the caller in a vector in the userdata context.  */
static enum
pkt_cb_resp key_file_walker (struct packet_walker *wlk, unsigned char tag,
						size_t packetsize, size_t hdrpos)
{
  struct key_data *kdat = (struct key_data *)(wlk->userdata);

  MESSAGE ("key packet %d size %d at offs $%04x kdat $%08x\n", tag,
						packetsize, hdrpos, kdat);

  if (tag != RFC4880_PT_PUBLIC_KEY)
    return pktCONTINUE;

  // So, get the data out.  Version is first.  In case of any errors during
  // parsing, we just discard the key and continue, hoping to find a good one.
  char ver = pkt_getch (wlk->pfile);
  if (ver != 4)
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, ver, "unsupported key version.");
      return pktCONTINUE;
    }

  // Only V4 accepted.  Discard creation time.
  if (pkt_getdword (wlk->pfile) == -1)
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, -1, "missing creation time.");
      return pktCONTINUE;
    }

  char pkalg = pkt_getch (wlk->pfile);
  if (pkalg != RFC4880_PK_DSA)
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, pkalg, "unsupported key alg.");
      return pktCONTINUE;
    }

  // Next, the four MPIs should be present.  Read them out,
  // convert to an s-expr and add that to the list.
  gcry_mpi_t p, q, g, y;
  p = q = g = y = 0;

  if ((pkt_get_mpi (&p, wlk->pfile) >= 0)
	&& (pkt_get_mpi (&q, wlk->pfile) >= 0)
	&& (pkt_get_mpi (&g, wlk->pfile) >= 0)
	&& (pkt_get_mpi (&y, wlk->pfile) >= 0))
    {
      // Convert to s-expr.
      gcry_sexp_t new_key;
      size_t n;

      gcry_error_t rv = gcry_sexp_build (&new_key, &n, dsa_pubkey_templ, p, q, g, y);
      if (rv != GPG_ERR_NO_ERROR)
	{
	  ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, rv, "while creating sig s-expr.");
	  return pktCONTINUE;
	}

#if CRYPTODEBUGGING
      // Debugging
      char sexprbuf[GPG_KEY_SEXPR_BUF_SIZE];
      n = gcry_sexp_sprint (new_key, GCRYSEXP_FMT_ADVANCED, sexprbuf,
							GPG_KEY_SEXPR_BUF_SIZE);
      msg ("key:%d\n'%s'", n, sexprbuf);
#endif /* CRYPTODEBUGGING */

      // Return it to caller in the vector.
      kdat->keys.push_back (new_key);
    }

  // Release temps and continue.
  if (p)
    gcry_mpi_release (p);
  if (q)
    gcry_mpi_release (q);
  if (g)
    gcry_mpi_release (g);
  if (y)
    gcry_mpi_release (y);

  return pktCONTINUE;
}

/*  Does what its name suggests: feeds a chosen amount of the data found
  at the current seek position in an io_stream into the message digest
  context passed in, using reasonably-sized chunks for efficiency.  */
static size_t
shovel_stream_data_into_md (io_stream *stream, size_t nbytes, gcry_md_hd_t md)
{
  const size_t TMPBUFSZ = 1024;
  unsigned char tmpbuf[TMPBUFSZ];
  size_t this_time, total = 0;
  ssize_t actual;
  MESSAGE ("shovel %d bytes at pos $%08x\n", nbytes, stream->tell ());
  while (nbytes)
    {
      this_time = (nbytes > TMPBUFSZ) ? TMPBUFSZ : nbytes;
      actual = stream->read (tmpbuf, this_time);
      if (actual <= 0)
        break;
      gcry_md_write (md, tmpbuf, actual);
      total += actual;
      nbytes -= actual;
      if (actual != (ssize_t)this_time)
        break;
    }
  return total;
}

/*  Canonicalise an s-expr by converting LFs to spaces so that
  it's all on one line and folding multiple spaces as we go.  */
static size_t
fold_lfs_and_spaces (char *buf, size_t n)
{
  char *ptr1 = buf, *ptr2 = buf;

  while (n--)
    {
      char ch = *ptr1++;
      if (ch == 0x0a)
	ch = ' ';
      *ptr2++ = ch;
      if (ch == 0x20)
	while (n && ((*ptr1 == ' ') || (*ptr1 == 0x0a)))
	  {
	    --n;
	    ++ptr1;
	  }
    }
  return ptr2 - buf;
}

/*  Do-nothing stubs called by the sig file walker to
  walk over the embedded subpackets.  In the event, we don't
  actually need to do this as we aren't inspecting them.  */
static enum
pkt_cb_resp hashed_subpkt_walker (struct packet_walker *wlk, unsigned char tag,
						size_t packetsize, size_t hdrpos)
{
  return pktCONTINUE;
}

static enum
pkt_cb_resp unhashed_subpkt_walker (struct packet_walker *wlk, unsigned char tag,
						size_t packetsize, size_t hdrpos)
{
  return pktCONTINUE;
}

/*  Callback to parse the packets found in the setup.ini/setup.bz2
  signature file.  We have to parse the header to get the hash type
  and other details.  Once we have that we can create a message
  digest context and start pumping data through it; first the ini
  file itself, then the portion of the packet itself that is
  covered by the hash.  */
 static enum
pkt_cb_resp sig_file_walker (struct packet_walker *wlk, unsigned char tag,
						size_t packetsize, size_t hdrpos)
{
  struct sig_data *sigdat = (struct sig_data *)(wlk->userdata);
  sigdat->complete = false;

  if (tag != RFC4880_PT_SIGNATURE)
    return pktCONTINUE;

  // To add the trailers later, we hang on to the current pos.
  size_t v34hdrofs = wlk->pfile->tell ();

  // So, get the data out.  Version is first.
  char ver = pkt_getch (wlk->pfile);
  if ((ver < 3) || (ver > 4))
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, ver, "unsupported sig version.");
      return pktHALT;
    }

  // Only V3 and V4 accepted.
  if (ver == 4)
    {
      sigdat->sig_type = pkt_getch (wlk->pfile);
      sigdat->pk_alg = pkt_getch (wlk->pfile);
      sigdat->hash_alg = pkt_getch (wlk->pfile);
    }
  else
    {
      int hmsize = pkt_getch (wlk->pfile);
      if (hmsize != RFC4880_SIGV3_HASHED_SIZE)
	{
	  ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, hmsize, "wrong hashed material size.");
	  return pktHALT;
	}
      v34hdrofs = wlk->pfile->tell ();
      if ((pkt_getch (wlk->pfile) < 0) || (pkt_getdword (wlk->pfile) == -1))
	{
	  ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, hmsize, "wrong hashed material size.");
	  return pktHALT;
	}
      if ((pkt_getdword (wlk->pfile) == -1) || (pkt_getdword (wlk->pfile) == -1))
	{
	  ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, -1, "missing signer ID.");
	  return pktHALT;
	}

      sigdat->sig_type = 0;
      sigdat->pk_alg = pkt_getch (wlk->pfile);
      sigdat->hash_alg = pkt_getch (wlk->pfile);
    }

  MESSAGE ("sig type %d, pk_alg %d, hash_alg %d\n", sigdat->sig_type,
						sigdat->pk_alg, sigdat->hash_alg);

  // We only handle binary file signatures
  if (sigdat->sig_type != RFC4880_ST_BINARY)
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, sigdat->sig_type, "unsupported sig type.");
      return pktHALT;
    }
  // And we only speak DSA.
  if (sigdat->pk_alg != RFC4880_PK_DSA)
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, sigdat->pk_alg, "unsupported pk alg.");
      return pktHALT;
    }

  // Start to hash all the data.  Figure out what hash to use.
  sigdat->algo = pkt_convert_hashcode (sigdat->hash_alg);
  if (sigdat->algo == GCRY_MD_NONE)
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, sigdat->hash_alg, "unconvertible hash.");
      return pktHALT;
    }

  // Now we know hash algo, we can create md context.
  gcry_error_t rv = gcry_md_open (&sigdat->md, sigdat->algo, 0);
  if (rv != GPG_ERR_NO_ERROR)
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, rv, "while initialising message digest.");
      return pktHALT;
    }

  // Add all the sig_file data into the hash.
  sigdat->sign_data->seek (0, IO_SEEK_SET);
  size_t nbytes = sigdat->sign_data->get_size ();
  if (nbytes != shovel_stream_data_into_md (sigdat->sign_data, nbytes, sigdat->md))
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, sigdat->hash_alg, "internal buffer error.");
      return pktHALT;
    }
  sigdat->sign_data->seek (0, IO_SEEK_SET);

  // V4 now has some hashed subpackets
  int hashed_subpkt_size = (ver == 4) ? pkt_getword (wlk->pfile) : 0;
  if (hashed_subpkt_size)
    pkt_walk_subpackets (wlk->pfile, hashed_subpkt_walker, wlk->owner,
		wlk->pfile->tell (), hashed_subpkt_size, wlk->userdata);

  // V4 now has some unhashed subpackets
  int unhashed_subpkt_size = (ver == 4) ? pkt_getword (wlk->pfile) : 0;
  if (unhashed_subpkt_size)
    pkt_walk_subpackets (wlk->pfile, unhashed_subpkt_walker, wlk->owner,
		wlk->pfile->tell (), unhashed_subpkt_size, wlk->userdata);

  // Both formats now have 16 bits of the hash value.
  int hash_first = pkt_getword (wlk->pfile);

  MESSAGE ("sig type %d, pk_alg %d, hash_alg %d - first $%04x\n", sigdat->sig_type,
			sigdat->pk_alg, sigdat->hash_alg, hash_first);

  /*    Algorithm-Specific Fields for DSA signatures:

     - MPI of DSA value r.

     - MPI of DSA value s.

   DSA signatures MUST use hashes that are equal in size to the number
   of bits of q, the group generated by the DSA key's generator value.  */

  if ((pkt_get_mpi (&sigdat->dsa_mpi_r, wlk->pfile) < 0)
      || (pkt_get_mpi (&sigdat->dsa_mpi_s, wlk->pfile) < 0))
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, "unpacking mpi.");
      return pktHALT;
    }

  MESSAGE ("Read sig packets succesfully!\n");

  // Now we got all the data out ok, rewind and hash the first trailer.
  wlk->pfile->seek (v34hdrofs, IO_SEEK_SET);
  nbytes = (ver == 4) ? (RFC4880_SIGV4_HASHED_OVERHEAD + hashed_subpkt_size) 
                      : (RFC4880_SIGV3_HASHED_SIZE);
  if (nbytes != shovel_stream_data_into_md (wlk->pfile, nbytes, sigdat->md))
    {
      ERRKIND (wlk->owner, IDS_CRYPTO_ERROR, sigdat->hash_alg, "internal buffer error 2.");
      return pktHALT;
    }

  if (ver == 4)
    {
      // And now the synthetic final trailer.
      gcry_md_putc (sigdat->md, 4);
      gcry_md_putc (sigdat->md, 0xff);
      gcry_md_putc (sigdat->md, (nbytes >> 24) & 0xff);
      gcry_md_putc (sigdat->md, (nbytes >> 16) & 0xff);
      gcry_md_putc (sigdat->md, (nbytes >> 8) & 0xff);
      gcry_md_putc (sigdat->md, nbytes & 0xff);
    }

  // Hooray, succeeded!
  sigdat->complete = true; 

  return pktHALT;
}

/*  Size and allocate a temp buffer to print a representation
  of a public key s-expr into, then add that to the extra keys
  setting so it persists for the next run.  */
void
add_key_from_sexpr (gcry_sexp_t key)
{
  size_t n = gcry_sexp_sprint (key, GCRYSEXP_FMT_ADVANCED, 0, ~0);
  char *sexprbuf = new char[n];
  n = gcry_sexp_sprint (key, GCRYSEXP_FMT_ADVANCED, sexprbuf, n);
  // +1 because we want to include the nul-terminator.
  n = fold_lfs_and_spaces (sexprbuf, n + 1);
  ExtraKeysSetting::instance().add_key (sexprbuf);
  MESSAGE ("keep:%d\n'%s'", n, sexprbuf);
  delete [] sexprbuf;
}

/*  Verify the signature on an ini file.  Takes care of all key-handling.  */
bool
verify_ini_file_sig (io_stream *ini_file, io_stream *ini_sig_file, HWND owner)
{
  /*  DSA public key in s-expr format.  */
  gcry_sexp_t dsa_key;

  /*  Data returned from packet walker.  */
  struct sig_data sigdat;

  /*  Vector of extra keys to use.  */
  std::vector<gcry_sexp_t> keys_to_try;

  /*  Vector of cached extra keys from last run.  */
  static std::vector<gcry_sexp_t> input_keys;

  /*  Overall status of signature.  */
  bool sig_ok = false;

  // Temps for intermediate processing.
  gcry_error_t rv;
  size_t n;

  /* Initialise the library.  */
  gcry_check_version (NULL);

  /* So first build the built-in key.  */
  rv = gcry_sexp_new (&dsa_key, cygwin_pubkey_sexpr, strlen (cygwin_pubkey_sexpr), 1);
  if (rv != GPG_ERR_NO_ERROR)
    {
      ERRKIND (owner, IDS_CRYPTO_ERROR, rv, "while creating pubkey s-expr.");
    }

#if CRYPTODEBUGGING
  char sexprbuf[GPG_KEY_SEXPR_BUF_SIZE];
  n = gcry_sexp_sprint (dsa_key, GCRYSEXP_FMT_ADVANCED, sexprbuf, GPG_KEY_SEXPR_BUF_SIZE);
  msg ("key:%d\n'%s'", n, sexprbuf);
#endif /* CRYPTODEBUGGING */

  /* Next we should extract the keys from the extrakeys user
  setting, and flush it; we'll only return them to it if they
  get used.  OTOH, should we do this at all?  The user settings
  file isn't heavily protected.  So we only trust the extra
  keys if we're told to by the user.  We still read them in
  and write them back out, which canonicalises and eliminates
  any duplicates or garbage lines that may have crept in.  */
  static bool input_keys_read = false;
  if (!input_keys_read)
    {
      // We only want to do this once, first time through:
      input_keys_read = true;
      // Copy all valid keys from ExtraKeysSetting into a
      // static vector where we can keep them throughout the
      // remainder of the run.
      for (size_t i = 0; i < ExtraKeysSetting::instance().num_keys (); i++)
	{
	  const char *keystring = ExtraKeysSetting::instance().get_key (i, &n);
	  gcry_sexp_t newkey;
	  rv = gcry_sexp_new (&newkey, keystring, n, 1);
	  if (rv == GPG_ERR_NO_ERROR)
	    input_keys.push_back (newkey);
	}

      // Now flush out the ExtraKeysSetting; from here on it
      // will build up a list of the keys we want to retain.
      ExtraKeysSetting::instance().flush ();

      // Which, if we aren't using them, means all the ones
      // we just read.
      if (KeepUntrustedKeysOption || !UntrustedKeysOption)
	{
	  std::vector<gcry_sexp_t>::iterator it;
	  for (it = input_keys.begin (); it < input_keys.end (); ++it)
	    add_key_from_sexpr (*it);
	}
    }

  /* Next, there may have been command-line options. */
  std::vector<std::string> SexprExtraKeyStrings = SexprExtraKeyOption;
  for (std::vector<std::string>::const_iterator it
					= SexprExtraKeyStrings.begin ();
		it != SexprExtraKeyStrings.end (); ++it)
    {
      MESSAGE ("key str is '%s'\n", it->c_str ());
      gcry_sexp_t dsa_key2 = 0;
      rv = gcry_sexp_new (&dsa_key2, it->c_str (), it->size (), 1);
      if (rv == GPG_ERR_NO_ERROR)
	{
	  // We probably want to add it to the extra keys setting
	  // if KeepUntrustedKeysOption is supplied.
	  if (KeepUntrustedKeysOption)
	    add_key_from_sexpr (dsa_key2);
#if CRYPTODEBUGGING
	  n = gcry_sexp_sprint (dsa_key2, GCRYSEXP_FMT_ADVANCED, sexprbuf,
						GPG_KEY_SEXPR_BUF_SIZE);
	  // +1 because we want to include the nul-terminator.
	  n = fold_lfs_and_spaces (sexprbuf, n + 1);
	  ExtraKeysSetting::instance().add_key (sexprbuf);
	  msg ("key2:%d\n'%s'", n, sexprbuf);
#endif /* CRYPTODEBUGGING */
	  keys_to_try.push_back (dsa_key2);
	}
      else
	{
	  ERRKIND (owner, IDS_CRYPTO_ERROR, rv, "invalid command-line pubkey s-expr.");
	}
    }

  /* Also, we may have to read a key(s) file. */
  std::vector<std::string> ExtraKeysFiles = ExtraKeyOption;
  for (std::vector<std::string>::const_iterator it
					= ExtraKeysFiles.begin ();
		it != ExtraKeysFiles.end (); ++it)
    {
      io_stream *keys = get_url_to_membuf (*it, owner);
      if (keys)
	{
	  struct key_data kdat;
	  pkt_walk_packets (keys, key_file_walker, owner, 0, keys->get_size (), &kdat);
	  // We now have a vector of (some/any?) keys returned from
	  // the walker; add them to the list to try.
	  while (!kdat.keys.empty ())
	    {
	      // We probably want to add it to the extra keys setting
	      // if KeepUntrustedKeysOption is supplied.
	      if (KeepUntrustedKeysOption)
		add_key_from_sexpr (kdat.keys.back ());
#if CRYPTODEBUGGING
	      n = gcry_sexp_sprint (kdat.keys.back (), GCRYSEXP_FMT_ADVANCED,
						sexprbuf, GPG_KEY_SEXPR_BUF_SIZE);
	      // +1 because we want to include the nul-terminator.
	      n = fold_lfs_and_spaces (sexprbuf, n + 1);
	      ExtraKeysSetting::instance().add_key (sexprbuf);
	      msg ("key3:%d\n'%s'", n, sexprbuf);
#endif /* CRYPTODEBUGGING */
	      keys_to_try.push_back (kdat.keys.back ());
	      kdat.keys.pop_back ();
	    }
	}
    }

  // We pass in a pointer to the ini file in the user context data,
  // which the packet walker callback uses to create a new hash
  // context preloaded with all the signature-covered data.
  sigdat.complete = false;
  sigdat.sign_data = ini_file;
  sigdat.dsa_mpi_r = sigdat.dsa_mpi_s = 0;
  sigdat.md = 0;
  pkt_walk_packets (ini_sig_file, sig_file_walker, owner, 0,
				ini_sig_file->get_size (), &sigdat);
  if (sigdat.complete)
    {
      /*  DSA sig coefficients in s-expr format.  */
      gcry_sexp_t dsa_sig;

      /*  DSA signature hash data in s-expr format.  */
      gcry_sexp_t dsa_hash;

      /* So, we have hashed all the data, and found the sig coefficients.
        Next stages are to finalise the hash, build everything into 
        s-exprs, and call the libgcrypt verification routine.  */

      rv = gcry_sexp_build (&dsa_sig, &n, dsa_sig_templ, sigdat.dsa_mpi_r,
								sigdat.dsa_mpi_s);
      if (rv != GPG_ERR_NO_ERROR)
	{
	  ERRKIND (owner, IDS_CRYPTO_ERROR, rv, "while creating sig s-expr.");
	  return false;
	}

      gcry_md_final (sigdat.md);

      // Make a temp mpi from the hash output, then an s-expr from that.
      gcry_mpi_t dsa_mpi_hash = 0;
      unsigned char *tmpbuf = gcry_md_read (sigdat.md, 0);
      size_t dlen = gcry_md_get_algo_dlen (sigdat.algo);
      rv = gcry_mpi_scan (&dsa_mpi_hash, GCRYMPI_FMT_USG, tmpbuf, dlen, 0UL);
      if (rv != GPG_ERR_NO_ERROR)
	{
	  ERRKIND (owner, IDS_CRYPTO_ERROR, rv, "while creating hash MPI.");
	  return false;
	}

      rv = gcry_sexp_build (&dsa_hash, &n, data_hash_templ, dsa_mpi_hash);
      if (rv != GPG_ERR_NO_ERROR)
	{
	  ERRKIND (owner, IDS_CRYPTO_ERROR, rv, "while creating hash s-expr.");
	  return false;
	}

#if CRYPTODEBUGGING
      n = gcry_sexp_sprint (dsa_sig, GCRYSEXP_FMT_ADVANCED, sexprbuf,
							GPG_KEY_SEXPR_BUF_SIZE);
      msg ("sig:%d\n'%s'", n, sexprbuf);
      n = gcry_sexp_sprint (dsa_hash, GCRYSEXP_FMT_ADVANCED, sexprbuf,
							GPG_KEY_SEXPR_BUF_SIZE);
      msg ("hash:%d\n'%s'", n, sexprbuf);
#endif /* CRYPTODEBUGGING */

      // Well, we're actually there!  Try it against the main key.
      rv = gcry_pk_verify (dsa_sig, dsa_hash, dsa_key);
      // If not that, try any supplied on the commandline.
      if (rv != GPG_ERR_NO_ERROR)
	{
	  std::vector<gcry_sexp_t>::iterator it;
	  for (it = keys_to_try.begin (); it < keys_to_try.end (); ++it)
	    {
	      MESSAGE ("Testing a key to try\n");
	      rv = gcry_pk_verify (dsa_sig, dsa_hash, *it);
	      if (rv != GPG_ERR_NO_ERROR)
		continue;
	      // Found it!  This key gets kept!
	      add_key_from_sexpr (*it);
	      break;
	    }

	  // We only use the untrusted keys if told to.
	  it = ((rv != GPG_ERR_NO_ERROR) 
	    && (KeepUntrustedKeysOption || UntrustedKeysOption))
		? input_keys.begin ()
		: input_keys.end ();
	  for ( ; it < input_keys.end (); ++it)
	    {
	      MESSAGE ("Testing an input key\n");
	      rv = gcry_pk_verify (dsa_sig, dsa_hash, *it);
	      if (rv != GPG_ERR_NO_ERROR)
		continue;
	      // Found it!  This key gets kept!
	      add_key_from_sexpr (*it);
	      break;
	    }
	}

      sig_ok = (rv == GPG_ERR_NO_ERROR);

#if CRYPTODEBUGGING
      gcry_err_code_t code;
      gcry_err_source_t src;
      code = gcry_err_code (rv);
      src = gcry_err_source (rv);
      msg ("Well, pk verify returned $%08x - code %d src %d\n", rv, code, src);
#endif /* CRYPTODEBUGGING */

      gcry_mpi_release (dsa_mpi_hash);
      gcry_sexp_release (dsa_sig);
      gcry_sexp_release (dsa_hash);
    }

  // Discard the temp data then.
  gcry_sexp_release (dsa_key);
  if (sigdat.dsa_mpi_r)
    gcry_mpi_release (sigdat.dsa_mpi_r);
  if (sigdat.dsa_mpi_s)
    gcry_mpi_release (sigdat.dsa_mpi_s);
  if (sigdat.md)
    gcry_md_close (sigdat.md);
  while (keys_to_try.size ())
    {
      gcry_sexp_release (keys_to_try.back ());
      keys_to_try.pop_back ();
    }

  return sig_ok;
}

