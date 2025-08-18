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

#ifndef SETUP_CHOOSE_CLI_H
#define SETUP_CHOOSE_CLI_H

#include "package_meta.h"

bool isManuallyWanted(packagemeta &pkg, packageversion &version);
bool isManuallyDeleted(packagemeta &pkg);
bool areBuildDependenciesWanted(packagemeta &pkg);

extern bool hasManualSelections;

#endif /* SETUP_CHOOSE_CLI_H */
