/*
 * Copyright (c) 2003, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

#ifndef SETUP_USERSETTING_H
#define SETUP_USERSETTING_H

/* A user-related settings */

class UserSetting {
  public:
  virtual ~UserSetting();
  virtual void load() = 0;
  virtual void save() = 0;
  protected:
  UserSetting();
  private:
  /* not implemented */
  UserSetting(UserSetting const &);
  UserSetting &operator=(UserSetting const &);
  friend class UserSettingTest;
};

#endif /* SETUP_USERSETTING_H */
