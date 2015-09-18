/*
 * Copyright (c) 2015 Red Hat, Inc.
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

#include "trigger.h"
#include "io_stream.h"
#include "LogSingleton.h"

// ---------------------------------------------------------------------------
// implements class Triggers
//
// This maintains a list of packages and pathnames. Whenever a file is added or
// removed, we should check it against this list, and if it matches, a trigger
// file named after the package is created
//
// At the moment, the pathname is a simple initial substring.  More complex
// matching using a regex could be added if needed.
//
// This list is anticipated to be very small, so a linear search is acceptable.
// ---------------------------------------------------------------------------

static const std::string triggerFilePrefix = "cygfile:///var/cache/setup-triggers/";
std::list <Trigger> Triggers::triggers;

void
Triggers::AddTrigger(std::string package, std::string pathprefix)
{
  Log (LOG_PLAIN) << "Adding trigger path '" << pathprefix << "' for package '" << package << "'" << endLog;
  Trigger t(package, pathprefix);
  triggers.push_front(t);
}

void
Triggers::CheckTriggers(std::string fn)
{
  Log (LOG_BABBLE) << "Checking '" << fn << "' against triggers" << endLog;
  for (TriggerList::const_iterator i = triggers.begin();
       i != triggers.end();
       i++)
    {
      // check if pathprefix is an initial substring of the fn
      if ((fn.size() >= i->pathprefix.size()) &&
          (fn.compare(0, i->pathprefix.size(), i->pathprefix) == 0))
        {
          std::string triggerFile = triggerFilePrefix + i->package;

          Log (LOG_PLAIN) << "Creating trigger file '" << triggerFile << "'" << endLog;

          // ensure the directory exists
          io_stream::mkpath_p(PATH_TO_DIR, triggerFilePrefix, 0644);

          // create the trigger file
          io_stream *tmp = io_stream::open (triggerFile, "wb", 0644);
          if (tmp == NULL)
            Log (LOG_PLAIN) << "Warning: Unable to create trigger file '" + triggerFile << "'" << endLog;
          else
            delete tmp;
        }
    }
}
