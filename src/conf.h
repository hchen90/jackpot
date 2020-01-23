/* $ @conf.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_CONF_H_
#define	_CONF_H_

#include <string>
#include <map>

class Conf {
public:
  Conf();
  ~Conf();
  bool open(const std::string& file);
  void update(const std::string& file);
  bool get(const std::string& sec, const std::string& key, std::string& value, bool gt = true);
  void set(const std::string& sec, const std::string& key, const std::string& value);
  void del(const std::string& sec, const std::string& key);
  void del(const std::string& sec);
private:
  std::map<std::string, std::map<std::string, std::string>*> _settings;
};

#endif	/* _CONF_H_ */
