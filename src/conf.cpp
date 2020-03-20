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
#include <regex>

#include "config.h"
#include "conf.h"

#define ANONYMOUS_KEY ".anonymous"

using namespace std;

Conf::Conf() { _settings.clear(); }

Conf::~Conf()
{
  for (auto& it : _settings) {
    if (it.second != nullptr) {
      it.second->clear();
    }
  }
}

bool Conf::open(const void* ptr, size_t len)
{
  if (ptr == nullptr) return false;
  if (len <= 0) return false;

  char* last = (char*) ptr + len;
  char* start = (char*) ptr;
  char* end = strchr(start, '\n');

  if (end == nullptr) end = strchr(start, 0);
  
  smatch sm;

  regex re_bla("[\t ]+*");
  regex re_cmt("[\t ]+*;.*");
  regex re_sec("\\[([A-Za-z_0-9]+)\\]");
  regex re_par("[\t ]+*([A-Za-z_0-9]+)[\t ]+*=[\t ]+*([^\t ]+)");

  string session = ANONYMOUS_KEY;

  _settings.clear();

  bool okay = false;

  while (start < last && end != nullptr) {
    if (start >= end) break;

    string line = string(start, end - start);

    if (regex_match(line, re_bla)) {
      // blank line
    } else if (regex_match(line, re_cmt)) {
      // comment
    } else if (regex_search(line, sm, re_sec) && sm.size() == 2) { // section
      session = sm[1];
    } else if (regex_search(line, sm, re_par) && sm.size() == 3) { // search kay-value pair
      if (_settings.find(session) == _settings.end()) {
        _settings.insert(make_pair(session, make_shared<map<string, string>>()));
      }
      auto lt = _settings.find(session);
      if (lt != _settings.end() && lt->second != nullptr) {
        lt->second->insert(make_pair(sm[1], sm[2]));
      }
    } else break;

    okay = true;

    for (start = end; *start == '\n'; ) start++;

    end = strchr(start, '\n');

    if (end == nullptr) end = strchr(start, '\0');
  }

  return okay;
}

bool Conf::open(const string& file)
{
  bool okay = false;
  ifstream fin(file.c_str(), ios::in);

  if (fin.good()) {
    string line, session = ANONYMOUS_KEY;
    smatch sm;
    regex re_bla("[\t ]+*");
    regex re_cmt("[\t ]+*;.*");
    regex re_sec("\\[([A-Za-z_0-9]+)\\]");
    regex re_par("[\t ]+*([A-Za-z_0-9]+)[\t ]+*=[\t ]+*([^\t ]+)");

    _settings.clear();

    while (true) {
      if (getline(fin, line)) {
        if (regex_match(line, re_bla)) continue; // skip blank line
        if (regex_match(line, re_cmt)) continue; // skip comment
        if (regex_search(line, sm, re_sec) && sm.size() == 2) { // search section
          session = sm[1];
        } else if (regex_search(line, sm, re_par) && sm.size() == 3) { // search kay = value pair
          if (_settings.find(session) == _settings.end()) {
            _settings.insert(make_pair(session, make_shared<map<string, string>>()));
          }
          auto lt = _settings.find(session);
          if (lt != _settings.end() && lt->second) {
            lt->second->insert(make_pair(sm[1], sm[2]));
          }
        } else break;
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
      if (it.second && ! it.second->empty()) {
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

// return total number of items in [section]
size_t Conf::get(const string& sec)
{
  auto it = _settings.find(sec);
  if (it != _settings.end() && it->second) return it->second->size();
  return 0;
}

// get key name by specifing index in [section]
bool Conf::get(const string& sec, size_t index, string& key)
{
  auto it = _settings.find(sec);
  if (it != _settings.end() && it->second) {
    for (auto& lt : *it->second) {
      if (index == 0) {
        key = lt.first;
        return true;
      } else {
        index--;
      }
    }
  }

  return false;
}

bool Conf::get(const string& sec, const string& key, string& value, bool gt)
{
  if (! gt && _settings.find(sec) == _settings.end()) {
    _settings.insert(make_pair(sec, make_shared<map<string, string>>()));
  }
  auto it = _settings.find(sec);
  if (it != _settings.end() && it->second) {
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
  if (it != _settings.end() && it->second) {
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
    if (it->second) {
      it->second->clear();
    }
    _settings.erase(it);
  }
}

/*end*/
