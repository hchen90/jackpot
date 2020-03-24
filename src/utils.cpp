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
#include <mutex>
#include <string>

#include "config.h"

#ifdef USE_SSTREAM
#include <sstream>
#endif

#include "utils.h"


static std::mutex _std_log_mutex, _std_err_mutex;

void utils::log(const std::string& fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  utils::log(fmt, ap);
  va_end(ap);
}

static bool _log_disp_timestamp = false;

void utils::log(const std::string& fmt, va_list ap)
{
  char buf[BUFSIZ];

  if (vsnprintf(buf, sizeof(buf), fmt.c_str(), ap) > 0) {
#ifdef USE_SSTREAM
    std::ostringstream oss;
#else
    std::string str;
#endif
    if (_log_disp_timestamp) {
#ifdef USE_SSTREAM
      oss << "[" << (uint32_t) time(nullptr) << "] " << buf;
#else
      str = "[";

      char suf[16];

      snprintf(suf, sizeof(suf), "%u", (uint32_t) time(nullptr));

      str += suf;
      str += "] ";
      str += buf;
#endif
    } else {
#ifdef USE_SSTREAM
      oss << buf;
#else
      str = buf;
#endif
    }

    _std_log_mutex.lock();
#ifdef USE_SSTREAM
    std::cout << "\r" << oss.str() << std::endl;
#else
    std::cout << "\r" << str << std::endl;
#endif
    _std_log_mutex.unlock();
  }
}

void utils::log_disp_timestamp(bool dts)
{
  _log_disp_timestamp = dts;
}

void utils::error(const std::string& fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  _std_err_mutex.lock();
  char* serr = strerror(errno);
  _std_err_mutex.unlock();

  if (serr != nullptr) {
    std::string format = fmt;
    format += " (";
    format += serr;
    format += ")";
    utils::log(format, ap);
  }

  va_end(ap);
}

void utils::dump(const void* ptr, size_t len)
{
  std::string str;

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

bool utils::token(const std::string& str, const std::string& delim, std::vector<std::string>& result)
{
  char* sss = new char[str.size() + 1];

  if (sss == nullptr) return false;

  memcpy(sss, str.c_str(), str.size());

  sss[str.size()] = 0;

  char* saveptr = nullptr;
  const char* dlm = delim.c_str();

  result.clear();

  for (char* ptr = sss; ; ptr = nullptr) {
    char* s = strtok_r(ptr, dlm, &saveptr);
    if (s != nullptr) {
      result.push_back(s);
    } else {
      break;
    }
  }

  delete[] sss;

  return ! result.empty();
}

std::string utils::chomp(const std::string& str)
{
  const char* origin = str.c_str();
  const char* start = origin;
  size_t len = str.size();

  while (*start == '\r' || *start == '\n' || *start == ' ' || *start == '\t') start++;

  len -= (size_t) (start - origin);

  if (len > 0) {
    const char* end,* last = start + len;

    for (end = start + len - 1; end > start && (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t'); end--) {
      if (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t') last = end;
    }

    len = last - start;
  } else return str;

  return std::string(start, len);
}

bool utils::filexts(const std::string& str, std::string& exts)
{
  const char* end = str.c_str() + str.size();
  char* dot = strrchr((char*) str.c_str(), '.');

  if (dot != nullptr && *dot == '.' && dot + 1 < end) {
    exts = dot + 1;
    return true;
  }

  return false;
}

/*end*/
