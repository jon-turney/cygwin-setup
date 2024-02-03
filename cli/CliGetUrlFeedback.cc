/*
 * Copyright (c) 2024 Jon Turney
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

#include "cli/CliFeedback.h"
#include "msg.h"
#include "resource.h"
#include <stdio.h>

void
CliFeedback::fetch_progress_disable(bool disable)
{
}

void
CliFeedback::fetch_init (const std::string &url, int length)
{
  max_bytes = length;
  printf("Fetching: %s\n", url.c_str());
  start_tics = GetTickCount ();
}

void
CliFeedback::fetch_set_length(int length)
{
  max_bytes = length;
}

void
CliFeedback::fetch_set_total_length(long long int total_length)
{
  total_download_bytes = total_length;
  total_download_bytes_sofar = 0;
}

void
CliFeedback::fetch_progress (int bytes)
{
  DWORD tics = GetTickCount ();
  if (tics == start_tics)       // to prevent division by zero
    return;
  if (tics < last_tics + 200)   // to prevent flickering updates
    return;
  last_tics = tics;

  double kbps = ((double)bytes) / (double)(tics - start_tics);

  if (max_bytes > 0)
    {
      int perc = (int)(100.0 * ((double)bytes) / (double)max_bytes);
      printf ("%d %%  (%dk/%dk)  %03.1f kB/s",
              perc, bytes / 1000, max_bytes / 1000, kbps);

    }
  else
    printf("%d  %2.1f kB/s", bytes, kbps);

  if (total_download_bytes > 0)
    {
      int total_perc = (int)(100.0 * ((double)total_download_bytes_sofar + bytes/
                                      (double)total_download_bytes));
      printf("%d %%", total_perc);
    }
  printf("\n");
}

void
CliFeedback::fetch_total_progress ()
{
}

void
CliFeedback::fetch_finish (int total_bytes)
{
  total_download_bytes_sofar += total_bytes;
}

void
CliFeedback::fetch_fatal (const char *filename, const char *err)
{
  ::fatal (NULL, IDS_ERR_OPEN_WRITE, filename, err);
}
