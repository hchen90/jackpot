/* ***
 * @ $utils.cpp
 * 
 * Copyright (C) 2020 Hsiang Chen
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * ***/
#include <cstdarg>
#include <cstring>
#include <iostream>

#include "utils.h"

void std::log(const std::string& format, ...)
{
  char buf[BUFSIZ];
  va_list ap;

  va_start(ap, format);
  if (vsnprintf(buf, sizeof(buf), format.c_str(), ap) > 0) {
    cout << "\r" << buf << endl;
  }
  va_end(ap);
}

void std::dump(const void* ptr, size_t len)
{
  string str;

  size_t i, l;

  for (i = 0, l = 16; i < len; i += 16) {
    if (len - i < 16) {
      l = len - i;
      break;
    }

    unsigned char buf[16], txt[17];

    memcpy(buf, (char*) ((char*) ptr + i), sizeof(buf));
    
    char suf[128];

    size_t x;

    for (x = 0; x < 16; x++)
      if (buf[x] < 0x20 || buf[x] > 0x7e) txt[x] = '.';
      else txt[x] = buf[x];
    txt[16] = 0;

    snprintf(suf, sizeof(suf), "%02x %02x %02x %02x %02x %02x %02x %02x - %02x %02x %02x %02x %02x %02x %02x %02x ; %s", \
    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], \
    buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], txt);
    
    str += suf;
  }

  if (l < 16) {
    unsigned char* p = (unsigned char*) ((char*) ptr + i);
    char suf[128],* s = suf;
    int n, k = sizeof(suf), j;

    for (j = 0; j < 16; j++) {
      if (j == 8) {
        s[0] = '-'; s[1] = ' ';
        s += 2; k -= 2;
      }
      if (i + j < len) {
        n = snprintf(s, k, "%02x ", p[j]);
      } else {
        s[0] = s[1] = s[2] = ' ';
        n = 3;
      }
      if (n < 0) break;
      else {
        s += n;
        k -= n;
      }
    }

    char txt[17];

    for (j = 0; j < 16; j++)
      if (p[j] < 0x20 || p[j] > 0x7e) txt[j] = '.';
      else txt[j] = p[j];
    txt[16] = 0;

    snprintf(s, k, "; %s", txt);

    str += suf;
  }

  log(str);
}
/*end*/
