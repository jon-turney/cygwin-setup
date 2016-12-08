/*
 * Copyright (c) 2016 Jon Turney
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

#ifndef SETUP_ACTIONLIST_H
#define SETUP_ACTIONLIST_H

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// interface to class ActionList
//
// a list of Actions possible on a package
// ---------------------------------------------------------------------------

class Action
{
 public:
  Action(const std::string &_name, int _id, bool _selected, bool _enabled) :
    name(_name),
    id(_id),
    selected(_selected),
    enabled(_enabled)
  { };

  std::string name;
  int id;
  bool selected;
  bool enabled;
};

typedef std::vector<Action> Actions;

class ActionList
{
 public:
  void add(const std::string &name, int id, bool selected, bool enabled)
  {
    Action act(name, id, selected, enabled);
    list.push_back(act);
  };
  Actions list;
};

#endif /* SETUP_ACTIONLIST_H */
