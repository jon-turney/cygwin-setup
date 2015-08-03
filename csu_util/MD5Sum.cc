/*
 * Copyright (c) 2004 Max Bowsher
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Max Bowsher
 */

#include "MD5Sum.h"
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>

MD5Sum::MD5Sum(const MD5Sum& source)
{
  *this = source;
}

MD5Sum&
MD5Sum::operator= (const MD5Sum& source)
{
  state = source.state;
  memcpy(digest, source.digest, sizeof(digest));
  internalData = 0;
  if (source.internalData)
  {
    internalData = new gcry_md_hd_t;
    *internalData = *(source.internalData);
  }
  return *this;
}

MD5Sum::~MD5Sum()
{
  if (internalData) delete internalData;
}

void
MD5Sum::set(const unsigned char digest[16])
{
  memcpy(this->digest, digest, sizeof(this->digest));
  state = Set;
  if (internalData) delete internalData;
}

void
MD5Sum::begin()
{
  if (internalData) delete internalData;
  internalData = new gcry_md_hd_t;
  state = Accumulating;
  gcry_md_open(internalData, GCRY_MD_MD5, 0);
}

void
MD5Sum::append(const unsigned char* data, int nbytes)
{
  if (!internalData)
    throw new std::logic_error("MD5Sum::append() called on an object not "
                               "in the 'Accumulating' state");
  gcry_md_write(*internalData, data, nbytes);
}

void
MD5Sum::finish()
{
  if (!internalData)
    throw new std::logic_error("MD5Sum::finish() called on an object not "
                               "in the 'Accumulating' state");
  memcpy(digest, gcry_md_read(*internalData, GCRY_MD_MD5), 16);
  state = Set;
  delete internalData; internalData = 0;
}

std::string
MD5Sum::str() const
{
  std::ostringstream hexdigest;

  for (int i=0; i<16; ++i )
    hexdigest << std::hex << std::setfill('0') << std::setw(2)
	      << static_cast<unsigned int>(digest[i]);
  return hexdigest.str();
}

bool
MD5Sum::operator == (const MD5Sum& other) const
{
  if (state != Set || other.state != Set)
    throw new std::logic_error("MD5Sum comparison attempted on operands not "
                               "in the 'Set' state");
  return (memcmp(digest, other.digest, sizeof(digest)) == 0);
}
