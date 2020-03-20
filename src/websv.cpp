/* ***
 * @ $websv.cpp
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
#include "websv.h"
#include "server.h"
#include "utils.h"

using namespace std;
using namespace utils;

WebSv::WebSv() : _latest(0), _done(false), _server(nullptr), _fd_cli(-1), _ssl(nullptr), _running(false), _port_from(0), _td_web(nullptr), _cv_cleanup(nullptr) {}

WebSv::~WebSv()
{
  stop();
  if (! _ip_from.empty() && _port_from > 0) {
    log("[%s:%u] closing connection", _ip_from.c_str(), _port_from);
  }
}

void WebSv::start(Server* srv, int fd, const string& ip_from, int port_from)
{
  if (! _running && (_td_web = new thread(websv_td, this, srv, fd, ip_from, port_from)) != nullptr) {
    _td_web->detach();
  }
}

void WebSv::stop()
{
  if (_running) {
    _done = true;
    _running = false;
    if (_server != nullptr) {
      if (_ssl != nullptr) {
        _server->_tls.close(_ssl);
        _ssl = nullptr;
      }
      if (_fd_cli > 0) {
        _server->_loc.close(_fd_cli); 
        _fd_cli = -1;
      }
    }
    if (_cv_cleanup != nullptr) {
      _cv_cleanup->notify_one();
    }
    if (_td_web != nullptr) {
      delete _td_web;
      _td_web = nullptr;
    }
  }
}

bool WebSv::done()
{
  return _done;
}

time_t WebSv::time()
{
  return _latest;
}

bool WebSv::init(Server* srv, int fd, const string& ip_from, int port_from, SSL* ssl)
{
  _running = true;

  _server = srv;
  _fd_cli = fd;
  _ssl = ssl;
  _ip_from = ip_from;
  _port_from = port_from;
  _cv_cleanup = &srv->_cv_cleanup;

  return true;
}

void WebSv::transfer()
{
  char buf[MAX(BUFSIZ, 1024)];
  size_t len;

  string cmd, path, ver;
  
  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(_fd_cli, &fds);

  while (_running) {
    bool end = false;

    _latest = ::time(nullptr);

    struct timeval tmv = { .tv_sec = _server->_ctxwrapper.timeout(), .tv_usec = 0 };
    
    switch (select(_fd_cli + 1, &fds, nullptr, nullptr, &tmv)) {
      case 0:
      case -1:
        end = true;
        break;
    }

    if (! end) {
      if (_ssl != nullptr) { // HTTPS
        if ((len = _server->_tls.read(_ssl, buf, sizeof(buf))) > 0) {
          if (_server->web_hdrinfo(buf, len, cmd, path, ver)) {
            if (_server->_norootfs) {
              if (cmd == "GET") {
                if (path == "/") _server->_tls.write(_ssl, (void*) DEF_CTX_SUCCESS, sizeof(DEF_CTX_SUCCESS) - 1);
                else _server->_tls.write(_ssl, (void*) DEF_CTX_NOTFOUND, sizeof(DEF_CTX_NOTFOUND) - 1);
              } else {
                _server->_tls.write(_ssl, (void*) DEF_CTX_BADREQUEST, sizeof(DEF_CTX_BADREQUEST) - 1);
              }
            } else {
              CPIOContent ctx;

              _server->_ctxwrapper.request(cmd, path, ver, ctx);
              _server->_tls.write(_ssl, ctx.c_ptr, ctx.c_len);
            }
          } else {
            break;
          }
        } else {
          break;
        }
      } else { // HTTP
        if ((len = _server->_loc.recv(_fd_cli, buf, sizeof(buf))) > 0) {
          if (_server->web_hdrinfo(buf, len, cmd, path, ver)) {
            if (_server->_norootfs) {
              if (cmd == "GET") {
                if (path == "/") _server->_loc.send(_fd_cli, DEF_CTX_SUCCESS, sizeof(DEF_CTX_SUCCESS) - 1);
                else _server->_loc.send(_fd_cli, DEF_CTX_NOTFOUND, sizeof(DEF_CTX_NOTFOUND) - 1);
              } else {
                _server->_loc.send(_fd_cli, DEF_CTX_BADREQUEST, sizeof(DEF_CTX_BADREQUEST) - 1);
              }
            } else {
              CPIOContent ctx;

              _server->_ctxwrapper.request(cmd, path, ver, ctx);
              _server->_loc.send(_fd_cli, ctx.c_ptr, ctx.c_len);
            }
          } else {
            break;
          }
        } else {
          break;
        }
      }
    } else break;
  }

  stop();
}

void WebSv::websv_td(WebSv* self, Server* srv, int fd, const string& ip_from, int port_from)
{
  if (self->init(srv, fd, ip_from, port_from)) {
    self->transfer();
  }
}

/*end*/
