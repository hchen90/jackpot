/* $ @conf.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_CONF_H_
#define	_CONF_H_

#include <string>
#include <map>

#include "config.h"

#ifdef USE_SMARTPOINTER
#include <memory>
#endif

class Conf {
public:
  Conf();
  ~Conf();
  bool open(const void* ptr, size_t len);
  bool open(const std::string& file);
  void update(const std::string& file);
  size_t get(const std::string& sec);
  bool get(const std::string& sec, size_t index, std::string& key);
  bool get(const std::string& sec, const std::string& key, std::string& value, bool gt = true);
  void set(const std::string& sec, const std::string& key, const std::string& value);
  void del(const std::string& sec, const std::string& key);
  void del(const std::string& sec);
private:
#ifdef USE_SMARTPOINTER
  std::map<std::string, std::shared_ptr<std::map<std::string, std::string>>> _settings;
#else
  std::map<std::string, std::map<std::string, std::string>*> _settings;
#endif
};

#endif	/* _CONF_H_ */
