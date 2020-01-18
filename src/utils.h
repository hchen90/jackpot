/* $ @utils.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_UTILS_H_
#define	_UTILS_H_

#include <string>

namespace std {
  void log(const std::string& format, ...);
  void dump(const void* ptr, size_t len);
};

#endif	/* _UTILS_H_ */
