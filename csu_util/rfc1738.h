/*
 * $Id$
 */

#ifndef SETUP_RFC1738_H
#define SETUP_RFC1738_H

#include <string>

std::string rfc1738_escape_part(const std::string &url);
std::string rfc1738_unescape(const std::string &s);

#endif /* SETUP_RFC1738_H */
