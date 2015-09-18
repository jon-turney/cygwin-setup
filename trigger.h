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

#ifndef TRIGGERS_H
#define TRIGGERS_H

#include <string>
#include <list>

class Trigger
{
 public:
  std::string package;
  std::string pathprefix;

  Trigger(std::string _package, std::string _pathprefix)
   : package(_package), pathprefix(_pathprefix)
    { };
};

class Triggers
{
 public:
  static void AddTrigger(std::string package, std::string pathprefix);
  static void CheckTriggers(std::string fn);

 private:
  typedef std::list <Trigger> TriggerList;
  static TriggerList triggers;
};

#endif /* TRIGGERS_H */
