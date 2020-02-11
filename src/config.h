/* $ @config.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_CONFIG_H_
#define	_CONFIG_H_

#define PACKAGE_NAME "jackpot"
#define PACKAGE_VERSION "1.3"
#define PACKAGE_URL "https://github.com/hchen90/jackpot.git"
#define PACKAGE_BUGREPORT "a.chenxiang.z@gmail.com"

#ifndef MAX
  #define MAX(s1,s2) (s1>s2?s1:s2)
#endif

#ifndef MIN
  #define MIN(s1,s2) (s1<s2?s1:s2)
#endif

#define BUFSIZE 256

#endif	/* _CONFIG_H_ */
