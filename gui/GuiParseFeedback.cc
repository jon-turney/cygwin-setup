/*
 * Copyright (c) 2000,2007 Red Hat, Inc.
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

#include "Exception.h"
#include "gui/GuiFeedback.h"

#include "ini.h"
#include "msg.h"
#include "resource.h"
#include "state.h"
#include "threebar.h"

extern ThreeBarProgressPage Progress;

void
GuiFeedback::parse_init ()
{
  Progress.SetText1 (IDS_PROGRESS_PARSING);
  Progress.SetText2 ("");
  Progress.SetText3 ("");
  Progress.SetText4 (IDS_PROGRESS_PROGRESS);

  lastpct = 0;
  yyerror_count = 0;
  yyerror_messages.clear ();
}

void
GuiFeedback::parse_finish ()
{
  Progress.SetText2 ("");
  Progress.SetText3 ("");
  Progress.SetText4 (IDS_PROGRESS_PACKAGE);
  Progress.SetBar1 (0);
}

void
GuiFeedback::progress (unsigned long const pos, unsigned long const max)
{
  if (!max)
    /* length not known or eof */
    return;
  if (lastpct == 100)
    /* rounding down should mean this only ever fires once */
    lastpct = 0;
  if (pos * 100 / max > lastpct)
    {
      lastpct = pos * 100 / max;
      /* Log (LOG_BABBLE) << lastpct << "% (" << pos << " of " << max
         << " bytes of ini file read)" << endLog; */
    }
  Progress.SetBar1 (pos, max);

  static char buf[100];
  sprintf (buf, "%d %%  (%ldk/%ldk)", lastpct, pos/1000, max/1000);
  Progress.SetText3 (buf);
}

void
GuiFeedback::iniName (const std::string& name)
{
  Progress.SetText2 (name.c_str ());
  Progress.SetText3 ("");
  filename = name;
}

void
GuiFeedback::babble (const std::string& message)const
{
  Log (LOG_BABBLE) << message << endLog;
}

void
GuiFeedback::warning (const std::string& message)const
{
  mbox (Progress.GetHWND(), message.c_str (), "Warning", 0);
}

void
GuiFeedback::note_error(int lineno, const std::string &error)
{
  char tmp[16];
  sprintf (tmp, "%d", lineno);

  std::string e = filename + " line " + tmp + ": " + error;

  if (!yyerror_messages.empty ())
    yyerror_messages += "\n";

  yyerror_messages += e;
  yyerror_count++;
}

bool
GuiFeedback::has_errors () const
{
  return (yyerror_count > 0);
}

void
GuiFeedback::show_errors () const
{
  mbox (Progress.GetHWND(), yyerror_messages.c_str (), "Parse Errors", 0);
}

static DWORD WINAPI
do_ini_thread_reflector (void* p)
{
  HANDLE *context;
  context = (HANDLE*)p;

  SetThreadUILanguage(langid);

  try
  {
    GuiFeedback feedback((HWND)context[1]);
    bool succeeded = do_ini_thread(feedback);

    // Tell the progress page that we're done downloading
    Progress.PostMessageNow (WM_APP_SETUP_INI_DOWNLOAD_COMPLETE, 0, succeeded);
  }
  TOPLEVEL_CATCH ((HWND) context[1], "ini");

  ExitThread (0);
}

static HANDLE context[2];

void
do_ini (HINSTANCE h, HWND owner)
{
  context[0] = h;
  context[1] = owner;

  DWORD threadID;
  CreateThread (NULL, 0, do_ini_thread_reflector, context, 0, &threadID);
}
