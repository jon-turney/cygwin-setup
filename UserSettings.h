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

/* A collection of user-related settings */

#ifndef _USERSETTINGS_H_
#define _USERSETTINGS_H_

#include <vector>

class UserSetting;
class UserSettings {
  public:
    static UserSettings &Instance();
    void registerSetting(UserSetting &);
    void deRegisterSetting(UserSetting &);
  private:
    static UserSettings Instance_;
    typedef std::vector<UserSetting *> Settings;
    void init();
    int inited;
    Settings settings;
};

#endif /* _USERSETTINGS_H_ */
