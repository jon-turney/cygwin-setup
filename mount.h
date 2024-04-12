/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2006, 2008, 2013 Red Hat, Inc.
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

#ifndef SETUP_MOUNT_H
#define SETUP_MOUNT_H
#include <string>
#include "String++.h"

#define SETUP_KEY_WOW64 ((installArch != IMAGE_FILE_MACHINE_I386) ? KEY_WOW64_64KEY : KEY_WOW64_32KEY)

void create_install_root ();
void read_mounts (const std::string);
void set_default_root_scope();

std::string cygpath (const std::string&);
void set_root_dir (const std::string);
const std::string &get_root_dir ();

#endif /* SETUP_MOUNT_H */
