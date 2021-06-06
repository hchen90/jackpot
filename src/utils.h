/* $ @utils.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_UTILS_H_
#define	_UTILS_H_

#include <string>
#include <vector>

namespace utils {
  void log(const char* fmt, ...);
  void log(const char* fmt, va_list ap);
  void log_disp_timestamp(bool dts);
  void error(const char* fmt, ...);
  void dump(const void* ptr, size_t len);
  bool token(const std::string& str, const std::string& delim, std::vector<std::string>& result);
  std::string chomp(const std::string& str);
  bool filexts(const std::string& str, std::string& exts);
};

#endif	/* _UTILS_H_ */
