//
// compactos.h
//
// Copyright (C) 2021 Christian Franke
//
// SPDX-License-Identifier: MIT
//

#ifndef COMPACTOS_H
#define COMPACTOS_H

#include <windows.h>

/* Not yet provided by w32api headers. */
#ifndef FILE_PROVIDER_COMPRESSION_XPRESS4K
#define FILE_PROVIDER_COMPRESSION_XPRESS4K  0
#define FILE_PROVIDER_COMPRESSION_LZX       1
#define FILE_PROVIDER_COMPRESSION_XPRESS8K  2
#define FILE_PROVIDER_COMPRESSION_XPRESS16K 3
#endif

// Returns: 1=compressed, 0=not compressed, -1=error
int CompactOsCompressFile(HANDLE h, DWORD algorithm);

#endif // COMPACTOS_H
