/*
 * Copyright (c) 2003, Robert Collins
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <rbtcollins@hotmail.com>
 *
 */

#include "UserSettings.h"

#include <assert.h>
#include <string.h>

int
main (int argc, char **argv)
{
  UserSettings settings;
  UserSettings& i = UserSettings::instance();
  i.set("key", "value");
  assert(strcmp(i.get("key"), "value") == 0);
  return 0;
}
