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
 *  This is the header for the ExtraKeysSetting class, which persists and reads
 * in (and saves) extra public DSA signing keys for the verification process.
 * It stores them all in a contiguous memory buffer.  Each is one line of
 * ASCII text terminated by LF.  THERE IS NO NUL-TERMINATION HERE, TAKE CARE!
 * The buffer is sized to the exact size of the content including the terminating
 * LF of the last entry.  There is no zero after it.  After reading the file,
 * any partial last line is truncated.
 *
 * Written by Dave Korn <dave.korn.cygwin@gmail.com>
 *
 */

#ifndef SETUP_KEYSSETTING_H
#define SETUP_KEYSSETTING_H

#include <string>

class ExtraKeysSetting
{
  private:
    char *keybuffer;
    size_t bufsize;
    size_t numkeys;
    static ExtraKeysSetting *global;
  public:
    // Loads keys from the "extrakeys" user setting.
    ExtraKeysSetting ();
    // Saves them back again.
    ~ExtraKeysSetting ();
    static ExtraKeysSetting& instance () {return *global;}

  private:
    // Extend (or shrink) allocated buffer.  Leaves it in a
    // potentially invalid state until count_keys is called
    // (or the bufsize is made valid by filling it up and
    // the numkeys count adjusted likewise).
    void realloc (size_t newbufsize);
    // Count keys and size buffer after loading.
    size_t count_keys (void);
    // Add a key without testing for existence
    void add_unique_key (const char *key);

  public:
    // Number of keys in buffer.
    size_t num_keys (void);
    // Get pointer to LF-terminated Nth. key string and len.
    const char *get_key (size_t num, size_t *size);
    // Add a key to the buffer (if not already present).
    void add_key (const char *key);
    // Empty the buffer.
    void flush (void);
};

#endif /* SETUP_CONNECTIONSETTING_H */
