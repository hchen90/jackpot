/* ***
 * @ $conf.cpp
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
#include <fstream>
#include <cstring>

#include "config.h"
#include "conf.h"

#define ANONYMOUS_KEY ".anonymous"

using namespace std;

Conf::Conf() {}

Conf::~Conf()
{
  for (auto& it : _settings) {
    if (it.second != nullptr) {
      it.second->clear();
      delete it.second;
    }
  }
}

bool Conf::open(const string& file)
{
  bool okay = false;
  ifstream fin(file.c_str(), ios::in);

  if (fin.good()) {
    string line, session = ANONYMOUS_KEY;

    while (true) {
      if (getline(fin, line)) {
        size_t l = line.size();
        const char* s = line.c_str();
        const char* e = s + l;

        if (*s == ';') continue; // skip comment
        if (l > 2 && *s == '[' && *(e - 1) == ']') { // session
          session = string(s + 1, l - 2);
        } else {
          if (_settings.find(session) == _settings.end()) {
            _settings.insert(make_pair(session, new map<string, string>()));
          }

          while (s < e && (*s == ' ' || *s == '\t') && *s != '\0') s++;
          if (*s != '\0') {
            const char* c = strchr(s, '=');
            if (*c == '=' && c + 1 < e) {
              const char* d = s;
              while (d < c && *d != ' ' && *d != '\t') d++;
              string key = string(s, d - s);
              const char* f = c + 1;
              while (f < e && (*f == ' ' || *s == '\t') && *f != '\0') f++;
              const char* g = strchr(f, ' ');
              if (g != nullptr) e = g;
              const char* h = strchr(f, '\t');
              if (h != nullptr) e = h;
              string value = string(f, e - f);

              auto lt = _settings.find(session);
              if (lt != _settings.end() && lt->second != nullptr) {
                lt->second->insert(make_pair(key, value));
              }
            }
          }
        }
        okay = true;
      } else break;
    }

    fin.close();
  }

  return okay;
}

void Conf::update(const string& file)
{
  ofstream fout(file.c_str(), ios::out);

  if (fout.good()) {
    for (auto& it : _settings) {
      if (it.second != nullptr && ! it.second->empty()) {
        if (it.first != ANONYMOUS_KEY) {
          fout << "[" << it.first << "]" << endl;
        }
        for (auto& lt : *it.second) {
          fout << lt.first << "=" << lt.second << endl;
        }
      }
    }
  }
}

bool Conf::get(const string& sec, const string& key, string& value, bool gt)
{
  if (! gt && _settings.find(sec) == _settings.end()) {
    _settings.insert(make_pair(sec, new map<string, string>()));
  }
  auto it = _settings.find(sec);
  if (it != _settings.end() && it->second != nullptr) {
    if (! gt && it->second->find(key) == it->second->end()) {
      it->second->insert(make_pair(key, value));
      return true;
    }
    auto lt = it->second->find(key);
    if (lt != it->second->end()) {
      if (gt) {
        value = lt->second;
      } else {
        lt->second = value;
      }
      return true;
    }
  }
  return false;
}

void Conf::set(const string& sec, const string& key, const string& value)
{
  string res = value;
  get(sec, key, res, false);
}

void Conf::del(const string& sec, const string& key)
{
  auto it = _settings.find(sec);
  if (it != _settings.end() && it->second != nullptr) {
    auto lt = it->second->find(key);
    if (lt != it->second->end()) {
      it->second->erase(lt);
    }
  }
}

void Conf::del(const string& sec)
{
  auto it = _settings.find(sec);
  if (it != _settings.end()) {
    if (it->second != nullptr) {
      it->second->clear();
      delete it->second;
    }
    _settings.erase(it);
  }
}

/*end*/
