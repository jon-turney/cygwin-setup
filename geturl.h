/*
 * Copyright (c) 2000, 2001, Red Hat, Inc.
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

#ifndef SETUP_GETURL_H
#define SETUP_GETURL_H

/* Download files from the Internet. */

#include <string>

class io_stream;
class Feedback;

io_stream *get_url_to_membuf (const std::string &_url, Feedback &feedback);
std::string get_url_to_string (const std::string &_url, Feedback &feedback);
int get_url_to_file (const std::string &_url, const std::string &_filename,
                     int expected_size, Feedback &feedback);

#endif /* SETUP_GETURL_H */
