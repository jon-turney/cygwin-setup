/*
 * Copyright (c) 2017 Jon Turney
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#ifndef PACKAGE_DEPENDS_H
#define PACKAGE_DEPENDS_H

#include <PackageSpecification.h>
#include <vector>

typedef std::vector <PackageSpecification *> PackageDepends;

void dumpPackageDepends (PackageDepends const &currentList, std::ostream &);

#endif // PACKAGE_DEPENDS_H
