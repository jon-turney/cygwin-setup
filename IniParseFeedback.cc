/*
 * Copyright (c) 2002 Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <robertc@hotmail.com>
 *
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "IniParseFeedback.h"

IniParseFeedback::~IniParseFeedback(){}

void IniParseFeedback::babble(String const &) const {}
void IniParseFeedback::warning (String const &) const {}
void IniParseFeedback::error(String const &) const {}
