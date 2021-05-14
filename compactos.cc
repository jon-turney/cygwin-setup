//
// compactos.cc
//
// Copyright (C) 2021 Christian Franke
//
// SPDX-License-Identifier: MIT
//

#include "compactos.h"

/* Not yet provided by w32api headers. */
#ifndef FSCTL_SET_EXTERNAL_BACKING
#define FSCTL_SET_EXTERNAL_BACKING \
  CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 195, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#endif

#ifndef WOF_CURRENT_VERSION
#define WOF_CURRENT_VERSION 1

typedef struct _WOF_EXTERNAL_INFO {
  DWORD Version;
  DWORD Provider;
} WOF_EXTERNAL_INFO;

#endif

#ifndef WOF_PROVIDER_FILE
#define WOF_PROVIDER_FILE 2
#define FILE_PROVIDER_CURRENT_VERSION 1

typedef struct _FILE_PROVIDER_EXTERNAL_INFO_V1 {
  DWORD Version;
  DWORD Algorithm;
  DWORD Flags;
} FILE_PROVIDER_EXTERNAL_INFO_V1;

#endif

#ifndef ERROR_COMPRESSION_NOT_BENEFICIAL
#define ERROR_COMPRESSION_NOT_BENEFICIAL 344
#endif

int CompactOsCompressFile(HANDLE h, DWORD algorithm)
{
  struct {
    WOF_EXTERNAL_INFO Wof;
    FILE_PROVIDER_EXTERNAL_INFO_V1 FileProvider;
  } wfp;
  wfp.Wof.Version = WOF_CURRENT_VERSION;
  wfp.Wof.Provider = WOF_PROVIDER_FILE;
  wfp.FileProvider.Version = FILE_PROVIDER_CURRENT_VERSION;
  wfp.FileProvider.Algorithm = algorithm;
  wfp.FileProvider.Flags = 0;

  if (!DeviceIoControl(h, FSCTL_SET_EXTERNAL_BACKING, &wfp, sizeof(wfp), 0, 0, 0, 0))
    {
      if (GetLastError() != ERROR_COMPRESSION_NOT_BENEFICIAL)
        return -1;
      return 0;
    }

  return 1;
}
