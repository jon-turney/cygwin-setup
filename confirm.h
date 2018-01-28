/*
 * Copyright (c) 2018
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

#ifndef SETUP_CONFIRM_H
#define SETUP_CONFIRM_H

#include "proppage.h"

class ConfirmPage:public PropertyPage
{
public:
  ConfirmPage ();
  virtual ~ConfirmPage () { };
  bool Create ();
  virtual void OnInit ();
  virtual void OnActivate ();
  virtual long OnNext ();
  virtual long OnBack ();
  virtual long OnUnattended ();
private:
  long whatNext ();
};

#endif /* SETUP_CONFIRM_H */
