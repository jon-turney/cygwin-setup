/*
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#ifndef SETUP_QUIET_H
#define SETUP_QUIET_H

#include "getopt++/StringChoiceOption.h"

typedef enum
{
  QuietUnattended,
  QuietNoInput,
  QuietHidden,
} QuietEnum;

extern StringChoiceOption UnattendedOption;

#endif /* SETUP_QUIET_H */
