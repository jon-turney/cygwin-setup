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
#include "win32.h"

// ---------------------------------------------------------------------------
// interface to class ActionList
//
// a list of Actions possible on a package
// ---------------------------------------------------------------------------

class Action
{
 public:
  Action(const std::wstring &_name, int _id, bool _selected, bool _enabled) :
    name(_name),
    id(_id),
    selected(_selected),
    enabled(_enabled)
  { };

  std::wstring name;
  int id;
  bool selected;
  bool enabled;
};

typedef std::vector<Action> Actions;

class ActionList
{
 public:
  void add(const std::wstring &name, int id, bool selected, bool enabled)
  {
    Action act(name, id, selected, enabled);
    list.push_back(act);
  };
  void add(unsigned int name_res, int id, bool selected, bool enabled)
  {
    const std::wstring name = LoadStringW(name_res);
    add(name, id, selected, enabled);
  };
  Actions list;
};

#endif /* SETUP_ACTIONLIST_H */
