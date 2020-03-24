/* ***
 * @ $ctxwrapper.cpp
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
#include "ctxwrapper.h"
#include "utils.h"

using namespace std;
using namespace utils;

/////////////////////////////////////////////////

#define DEF_HDR_BADREQUEST "HTTP/1.1 400 Bad Request"
#define DEF_HDR_NOTFOUND "HTTP/1.1 404 Not Found"
#define DEF_HDR_SUCCESS "HTTP/1.1 200 OK"
#define DEF_HDR_REDIRECT "HTTP/1.1 301 Moved Permanently"

#define DEF_BOD_BADREQUEST "<html><head><title>Bad Request</title></head><body><h1>400 Bad Request</h1><p>Unknown request</p></body></html>"
#define DEF_BOD_NOTFOUND "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>File cannot be found</p></body></html>"
#define DEF_BOD_SUCCESS "<html><head><title>Welcome</title><h1>Welcome</h1><p>This page is only for test</p></head></html>"

/*
uint16_t statuscode[] = {
  100, // continue
  101, // switching protocols
  200, // okay
  201, // created
  202, // accepted
  203, // non-authoritative information
  204, // no content
  205, // reset content
  206, // partial content
  300, // multiple choices
  301, // moved permanently
  302, // found
  303, // see other
  304, // not mofidied
  305, // use proxy
  307, // temporary redirect
  400, // bad request
  401, // unauthorized
  402, // payment required
  403, // forbidden
  404, // not found
  405, // modthod not allowed
  406, // not acceptable
  407, // proxy authentication required
  408, // request timeout
  409, // conflict
  410, // gone
  411, // length required
  412, // precondition failed
  413, // request entity too large
  414, // request-url too long
  415, // unsupported media type
  416, // requested range not satisfiable
  417, // expectation failed
  500, // internal server error
  501, // not implemented
  502, // bad gateway
  503, // service unavaliable
  504, // gateway timeout
  505, // http version not supported
};
*/

/////////////////////////////////////////////////

CtxFile::CtxFile() {}

CtxFile::~CtxFile()
{
  _ctx.clear();
}

void CtxFile::clear()
{
  _ctx.clear();
}

/////////////////////////////////////////////////

CtxWrapper::CtxWrapper() : _timeout(DEF_CTIMEOUT) {}

CtxWrapper::~CtxWrapper() {}

bool CtxWrapper::opencpio(const string& filename)
{
  _rootfs.clear();

  bool ret = open(filename);

  if (ret) updateinfo();

  return ret;
}

bool CtxWrapper::updatecpio(const string& filename)
{
  return update(filename);
}

bool CtxWrapper::updatecpio()
{
  return update();
}

void CtxWrapper::closecpio()
{
  if (! _rootfs.empty()) {
#ifndef USE_SMARTPOINTER
    for (auto& it : _rootfs) delete it.second;
#endif
    _rootfs.clear();
  }
}

void CtxWrapper::gethead(uint16_t errcode, string& errstr)
{
  auto it = _schead.find(errcode);
  if (it != _schead.end()) {
    errstr = chomp(it->second);
  }
}

void CtxWrapper::getbody(uint16_t errcode, string& errstr)
{
  auto it = _scbody.find(errcode);
  if (it != _scbody.end()) {
    errstr = it->second;
  }
}

void CtxWrapper::request(const string& cmd, const string& pathname, const string& version, CPIOContent& result)
{
  string scstr, scbod;
  
  if (cmd == "GET") {
    if (getcontent(pathname, result)) {
      return;
    } else {
      scstr = DEF_HDR_NOTFOUND;
      gethead(404, scstr);
      scbod = DEF_BOD_NOTFOUND;
      getbody(404, scbod);
    }
  } else {
    scstr = DEF_HDR_BADREQUEST;
    gethead(400, scstr);
    scbod = DEF_BOD_BADREQUEST;
    getbody(400, scbod);
  }

  char cnl[BUFSIZE];

  if (_timeout > 0) {
    snprintf(cnl, sizeof(cnl), "\r\nConnection: keep-alive\r\nKeep-Alive: timeout=%lu\r\nContent-Type: text/html\r\nContent-Length: %lu\r\n\r\n", _timeout, scbod.size());
  } else {
    snprintf(cnl, sizeof(cnl), "\r\nContent-Type: text/html\r\nContent-Length: %lu\r\n\r\n", scbod.size());
  }

  scstr += cnl;
  scstr += scbod;

  result.clear();
  result.c_len = scstr.size();
  result.c_ptr = new uint8_t[result.c_len];

  if (result.c_ptr != nullptr) {
    memcpy(result.c_ptr, scstr.data(), result.c_len);
  }
}

bool CtxWrapper::getcontent(const string& filename, CPIOContent& content)
{
  CPIOAttr attr;
  CPIOContent ctx;

  char cnl[BUFSIZE];

  if (getfile(filename, attr, ctx)) {
    if ((attr.c_stat.st_mode & 0770000) == C_ISREG) {
      return cons200resp(mimetype(filename), DEF_HDR_SUCCESS, ctx, content);
    } else if ((attr.c_stat.st_mode & 0770000) == C_ISLNK && ctx.c_ptr != nullptr) {
      string scstr = DEF_HDR_REDIRECT;

      gethead(301, scstr);

      if (_timeout > 0) {
        snprintf(cnl, sizeof(cnl), "\r\nConnection: keep-alive\r\nKeep-Alive: timeout=%lu\r\nLocation: %s\r\nContent-Length: 0\r\n\r\n", _timeout, ctx.c_ptr);
      } else {
        snprintf(cnl, sizeof(cnl), "\r\nLocation: %s\r\nContent-Length: 0\r\n\r\n", ctx.c_ptr);
      }

      scstr += cnl;

      content.clear();
      content.c_len = scstr.size();
      content.c_ptr = new uint8_t[content.c_len];

      if (content.c_ptr != nullptr) {
        memcpy(content.c_ptr, scstr.data(), content.c_len);
        return true;
      }
    } else if ((attr.c_stat.st_mode & 0770000) == C_ISDIR) {
      for (auto& it : _indexfl) {
        string fl = filename + it;
        if (getfile(fl, attr, ctx)) {
          return cons200resp(mimetype(fl), DEF_HDR_SUCCESS, ctx, content);
        }
      }
    }
  }

  return false;
}

bool CtxWrapper::getfile(const string& filename, CPIOAttr& attr, CPIOContent& ctx)
{
  string flnm = filepath(filename);

  if (flnm == "/.config") return false;

  if (flnm.empty() || flnm == "/") {
    for (auto& it : _indexfl) {
      if (_rootfs.find(it) != _rootfs.end()) {
        flnm = it;
        break;
      }
    }
  }

  auto it = _rootfs.find(flnm);

  if (it != _rootfs.end() && it->second->_visible) {
    attr = it->second->_attr;
    ctx = it->second->_ctx;
    return true;
  }

  return false;
}

bool CtxWrapper::setfile(const string& filename, CPIOAttr& attr, CPIOContent& ctx)
{
  string flnm = filepath(filename);

  if (flnm == "/.config") return false;

  auto it = _rootfs.find(flnm);

  if (it != _rootfs.end()) {
    it->second->_attr = attr;
    it->second->_ctx = ctx;
    return true;
  } else {
#ifdef USE_SMARTPOINTER
    shared_ptr<CtxFile> fobj = make_shared<CtxFile>();
    if (fobj)
#else
    CtxFile* fobj = new CtxFile();
    if (fobj != nullptr)
#endif
    {
      fobj->_attr = attr;
      fobj->_ctx = ctx;
      fobj->_visible = true;
      _rootfs.insert(make_pair(flnm, fobj));
      updateinfo();
      return true;
    }
  }

  return false;
}

bool CtxWrapper::delfile(const string& filename)
{
  string flnm = filepath(filename);

  if (flnm == "/.config") return false;

  auto it = _rootfs.find(flnm);

  if (it != _rootfs.end()) {
#ifndef USE_SMARTPOINTER
    delete it->second;
#endif
    _rootfs.erase(it);
    return true;
  }

  return false;
}

time_t& CtxWrapper::timeout()
{
  return _timeout;
}

bool CtxWrapper::input(string& filename, CPIOAttr& attr, CPIOContent& ctx)
{
#ifdef USE_SMARTPOINTER
  shared_ptr<CtxFile> fobj = make_shared<CtxFile>();
  if (fobj)
#else
  CtxFile* fobj = new CtxFile();
  if (fobj != nullptr)
#endif
  {
    string flnm = filepath(filename);

    fobj->_attr = attr;
    fobj->_ctx = ctx;
    fobj->_visible = true;

    _rootfs.insert(make_pair(flnm, fobj));
    return true;
  } else return false;
}

bool CtxWrapper::output(string& filename, CPIOAttr& attr, CPIOContent& ctx)
{
  if (_rootfs.empty()) return false;

  auto it = _rootfs.begin();

  if (it != _rootfs.end()) {
#ifdef USE_SMARTPOINTER
    if (it->second)
#else
    if (it->second != nullptr)
#endif
    {
      filename = it->first;
      attr = it->second->_attr;
      ctx = it->second->_ctx;
#ifndef USE_SMARTPOINTER
      delete it->second;
#endif
    }
    _rootfs.erase(it);
  } else return false;

  return true;
}

string CtxWrapper::mimetype(const string& filename)
{
  string ext;

  string pathname = filepath(filename);

  if (pathname == "/") return "text/html";

  if (filexts(pathname, ext)) {
    auto it = _mimetype.find(ext);
    if (it != _mimetype.end()) {
      return it->second;
    }
  }

  if (! ext.empty()) {
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "js") return "text/javascript";
    if (ext == "css") return "text/css";
    if (ext == "txt") return "text/plain";
    if (ext == "apng") return "image/apng";
    if (ext == "bmp") return "image/bmp";
    if (ext == "gif") return "image/gif";
    if (ext == "ico" || ext == "cur") return "image/x-icon";
    if (ext == "jpg" || ext == "jpeg" || ext == "jfif" || ext == "pjepg" || ext == "pjp") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "tif" || ext == "tiff") return "image/tiff";
    if (ext == "webp") return "image/webp";
    if (ext == "au" || ext == "snd") return "audio/basic";
    if (ext == "mid" || ext == "rmi") return "audio/mid";
    if (ext == "mp3") return "audio/mpeg";
    if (ext == "mp4") return "video/mp4";
    if (ext == "mp2" || ext == "mpa" || ext == "mpeg" || ext == "mpg" || ext == "mpv2") return "video/mpeg";
    if (ext == "mov" || ext == "qt") return "video/quicktime";
    if (ext == "flv") return "video/x-flv";
    if (ext == "avi") return "video/x-msvideo";
    if (ext == "swf") return "application/x-shockwave-flash";
    if (ext == "ps" || ext == "eps" || ext == "ai") return "application/postscript";
    if (ext == "pdf") return "application/pdf";
    if (ext == "doc") return "application/msword";
    if (ext == "ppt") return "application/vnd.ms-powerpoint";
    if (ext == "xls") return "application/vnd.ms-excel";
    if (ext == "json") return "application/json";
    if (ext == "ttf") return "font/ttf";
    if (ext == "otf") return "font/otf";
    if (ext == "woff") return "font/woff";
    if (ext == "zip") return "application/zip";
  }

  return "application/octet-stream";
}

string CtxWrapper::filepath(const string& filename)
{
  string ret;
  vector<string> result;

  string fn = filename;

  const char* ptr = filename.c_str();

  if (ptr != nullptr) {
    char* sp = strchr((char*) ptr, '?');
    if (sp != nullptr && sp > ptr) {
      fn = string(ptr, sp - ptr);
    }
  }

  if (token(fn, "/", result)) {
    vector<string> path;

    while (!result.empty()) {
      auto it = result.begin();
      if (it != result.end()) {
        if ((*it) == "..") {
          if (!path.empty()) {
            path.pop_back();
          }
        } else if ((*it) == ".") {
        } else {
          if (!(*it).empty()) {
            path.push_back(*it);
          }
        }
        result.erase(it);
      } else break;
    }

    for (auto& it : path) {
      ret += "/";
      ret += it;
    }
  } else {
    ret.clear();
    if (filename[0] != '/') {
      ret += "/";
    }
    ret += filename;
  }

  return ret;
}

void CtxWrapper::updateinfo()
{
  auto it = _rootfs.find("/.config");

  if (it != _rootfs.end()) {
    if (_config.open(it->second->_ctx.c_ptr, it->second->_ctx.c_len)) {
      // initialize config
      char key[4];
      uint8_t count = 0;
      string value;
      bool tri[2] = { false, false };

      _indexfl.clear();
      _hidefl.clear();

      while (true) {
        if (snprintf(key, sizeof(key), "%u", count++) > 0) {
          if (_config.get("index", key, value)) _indexfl.push_back(value);
          else tri[0] = true;
          if (_config.get("hide", key, value)) _hidefl.push_back(value);
          else tri[1] = true;
          if (tri[0] && tri[1]) break;
        } else break;
      }
      
      for (auto& lt : _hidefl) {
        auto ft = _rootfs.find(lt);
        if (ft != _rootfs.end()) {
          ft->second->_visible = false;
        }
      }

      size_t len;

      _schead.clear();
      _scbody.clear();

      for (int a = 0; a < 2; a++) {
        string sec, key;

        map<uint16_t, string>* scm;

        if (a > 0) {
          sec = "head";
          scm = &_schead;
        } else {
          sec = "error";
          scm = &_scbody;
        }

        if ((len = _config.get(sec)) > 0) {
          for (size_t idx = 0; idx < len; idx++) {
            if (_config.get(sec, idx, key) && _config.get(sec, key, value)) {
              auto it = _rootfs.find(value);
              if (it != _rootfs.end() && it->second->_ctx.c_ptr != nullptr) {
                string str = string((char*) it->second->_ctx.c_ptr, it->second->_ctx.c_len);
                scm->insert(make_pair((uint16_t) atoi(key.c_str()), str));
              }
            }
          }
        }
      }

      _mimetype.clear();

      if ((len = _config.get("mime")) > 0) {
        string key;
        
        for (size_t idx = 0; idx < len; idx++) {
          if (_config.get("mime", idx, key) && _config.get("mime", key, value)) {
            vector<string> result;

            if (token(value, ";,", result)) {
              for (auto& lt : result) {
                _mimetype.insert(make_pair(lt, key));
              }
            }
          }
        }
      }

    }
  }
}

/*end*/
