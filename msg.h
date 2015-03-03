/*
 * Copyright (c) 2000, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

#ifndef SETUP_MSG_H
#define SETUP_MSG_H

#include "win32.h"

/* This pops up a dialog with text from the string table ("id"), which
   is interpreted like printf.  The program exits when the user
   presses OK. */

void fatal (HWND owner, int id, ...);

/* Similar, but the program continues when the user presses OK */

void note (HWND owner, int id, ...);

/* returns IDYES or IDNO, otherwise same as note() */
int yesno (HWND owner, int id, ...);

/* general MessageBox() wrapper which understands unattended mode */
int mbox (HWND owner, const char *buf, const char *name, int type);

#endif /* SETUP_MSG_H */
