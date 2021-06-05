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

#include "conf.h"

#define ANONYMOUS_SECTION ".anonymous"
#define REGEX_BLANK "([\t ]+)?"
#define REGEX_COMMENT REGEX_BLANK ";.*"
#define REGEX_SECTION REGEX_BLANK "\\[([^\r\n\t ]+)\\]" REGEX_BLANK
#define REGEX_PAIR REGEX_BLANK "([^\r\n\t ]+)" REGEX_BLANK "=" REGEX_BLANK "([^\r\n]+)"

using namespace std;

Conf::Conf() { _settings.clear(); }

Conf::~Conf()
{
  for (auto& it : _settings) {
#ifdef USE_SMARTPOINTER
    if (it.second)
#else
    if (it.second != nullptr)
#endif
    {
      it.second->clear();
#ifndef USE_SMARTPOINTER
      delete it.second;
#endif
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

  regex re_bla(REGEX_BLANK);
  regex re_cmt(REGEX_COMMENT);
  regex re_sec(REGEX_SECTION);
  regex re_par(REGEX_PAIR);

  string session = ANONYMOUS_SECTION;

  _settings.clear();

  bool okay = false;

  while (start < last && end != nullptr) {
    if (start >= end) break;

    string line = string(start, end - start);

    if (regex_match(line, re_bla)) {
      // blank line
    } else if (regex_match(line, re_cmt)) {
      // comment
    } else if (regex_search(line, sm, re_sec) && sm.size() == 4) { // section
      session = sm[2];
    } else if (regex_search(line, sm, re_par) && sm.size() == 6) { // search kay-value pair
      if (_settings.find(session) == _settings.end()) {
#ifdef USE_SMARTPOINTER
        auto mp = make_shared<map<string, string>>();
        if (mp) {
          mp->clear();
          _settings.insert(make_pair(session, mp));
        }
#else
        auto mp = new map<string, string>();
        if (mp != nullptr) {
          mp->clear();
          _settings.insert(make_pair(session, mp));
        }
#endif
      }
      auto lt = _settings.find(session);
      if (lt != _settings.end() && lt->second != nullptr) {
        lt->second->insert(make_pair(sm[2], sm[5]));
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
    string line, session = ANONYMOUS_SECTION;
    
    smatch sm;

    regex re_bla(REGEX_BLANK);
    regex re_cmt(REGEX_COMMENT);
    regex re_sec(REGEX_SECTION);
    regex re_par(REGEX_PAIR);

    _settings.clear();

    while (true) {
      if (getline(fin, line)) {
        if (regex_match(line, re_bla)) continue; // skip blank line
        if (regex_match(line, re_cmt)) continue; // skip comment
        if (regex_search(line, sm, re_sec) && sm.size() == 4) { // search section
          session = sm[2];
        } else if (regex_search(line, sm, re_par) && sm.size() == 6) { // search kay = value pair
          if (_settings.find(session) == _settings.end()) {
#ifdef USE_SMARTPOINTER
            auto mp = make_shared<map<string, string>>();
            if (mp) {
              mp->clear();
              _settings.insert(make_pair(session, mp));
            }
#else
            auto mp = new map<string, string>();
            if (mp != nullptr) {
              mp->clear();
              _settings.insert(make_pair(session, mp));
            }
#endif
          }
          auto lt = _settings.find(session);
          if (lt != _settings.end() && lt->second) {
            lt->second->insert(make_pair(sm[2], sm[5]));
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
#ifdef USE_SMARTPOINTER
      if (it.second && ! it.second->empty())
#else
      if (it.second != nullptr && ! it.second->empty())
#endif
      {
        if (it.first != ANONYMOUS_SECTION) {
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
#ifdef USE_SMARTPOINTER
  if (it != _settings.end() && it->second)
#else
  if (it != _settings.end() && it->second != nullptr)
#endif
    return it->second->size();
  return 0;
}

// get key name by specifing index in [section]
bool Conf::get(const string& sec, size_t index, string& key)
{
  auto it = _settings.find(sec);
#ifdef USE_SMARTPOINTER
  if (it != _settings.end() && it->second)
#else
  if (it != _settings.end() && it->second != nullptr)
#endif
  {
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
#ifdef USE_SMARTPOINTER
    auto mp = make_shared<map<string, string>>();
    if (mp) {
      mp->clear();
      _settings.insert(make_pair(sec, mp));
    }
#else
    auto mp = new map<string, string>();
    if (mp != nullptr) {
      mp->clear();
      _settings.insert(make_pair(sec, mp));
    }
#endif
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
  if (it != _settings.end()) {
#ifdef USE_SMARTPOINTER
    if (it->second)
#else
    if (it->second != nullptr)
#endif
    {
      auto lt = it->second->find(key);
      if (lt != it->second->end()) {
        it->second->erase(lt);
      }
    }
  }
}

void Conf::del(const string& sec)
{
  auto it = _settings.find(sec);
  if (it != _settings.end()) {
#ifdef USE_SMARTPOINTER
    if (it->second)
#else
    if (it->second != nullptr)
#endif
    {
      it->second->clear();
#ifndef USE_SMARTPOINTER
      delete it->second;
#endif
    }
    _settings.erase(it);
  }
}

/*end*/
