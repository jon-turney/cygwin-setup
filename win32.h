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
 * and Robert Collins <rbtcollins@hotmail.com>
 *
 */

#ifndef SETUP_WIN32_H
#define SETUP_WIN32_H

/* Any include of <windows.h> should be through this file, which wraps it in
 * various other handling. */

/* Basic Windows features only. */
#define WIN32_LEAN_AND_MEAN

/* libstdc++-v3 _really_ dislikes min & max defined as macros. */
/* As of gcc 3.3.1, it defines NOMINMAX itself, so test first,
 * to avoid a redefinition error */
#ifndef NOMINMAX
#define NOMINMAX
#endif

/* In w32api 3.1, __declspec(dllimport) decoration is added to
 * certain symbols. This breaks our autoload mechanism - the symptom is
 * multiple declaration errors at link time. This define turns that off again.
 * It will default to off again in later w32api versions, but we need to work
 * with 3.1 for now. */
#define WINBASEAPI

/* Require at least Internet Explorer 3, in order to have access to 
 * sufficient Windows Common Controls features from <commctrl.h> . */
#define _WIN32_IE 0x0300

#include <windows.h>

/* FIXME: The use of _access(fname, 0) as an existence check should be
 * replaced with a more readable idiom, and this fragment removed. */
#ifndef _access
#define _access access
#endif

#ifdef __cplusplus
inline bool IsWindowsNT() { return GetVersion() < 0x80000000; }
#endif

#endif /* SETUP_WIN32_H */
