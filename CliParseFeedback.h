/*
 * Copyright (c) 2020 Jon Turney
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

#include "IniParseFeedback.h"

class CliParseFeedback : public IniParseFeedback
{
public:
  virtual void progress (unsigned long const pos, unsigned long const max);
  virtual void iniName (const std::string& name);
  virtual void babble (const std::string& message) const;
  virtual void warning (const std::string& message) const;
  virtual void show_errors () const;
  virtual void note_error(int lineno, const std::string &s);
  virtual bool has_errors () const;
private:
  int error_count = 0;
};
