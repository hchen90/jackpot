/* ***
 * @ $socks5.cpp
 * 
 * Copyright (C) 2019 Hsiang Chen
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
#include <cstring>

#include "config.h"
#include "socks5.h"
#include "utils.h"

#define DEF_TIMEOUT 20

using namespace std;

SOCKS5::SOCKS5() : _fd_tls(-1), _port_from(0), _done(false), _multi(false), _running(false), _stage(STAGE_INIT), _timeout(DEF_TIMEOUT), _nmpwd(nullptr), _tls(nullptr), _ssl(nullptr), _w_tls(nullptr), _w_tgt(nullptr), _w_tmo(nullptr), _td_tls(nullptr), _td_tgt(nullptr) {}

SOCKS5::~SOCKS5() { cleanup(); }

bool SOCKS5::init(TLS* tls, SSL* ssl, int fd, const std::string& ip_from, int port_from)
{
  _tls = tls;
  _ssl = ssl;
  _fd_tls = fd;
  _ip_from = ip_from;
  _port_from = port_from;

  if (_tls != nullptr && _ssl != nullptr && _fd_tls != -1)
    return true;
  else
    return false;
}

void SOCKS5::start(bool multi, float tmo)
{
  _multi = multi;
  _timeout = tmo;
  _running = true;

  if (multi) {
    if ((_td_tls = new thread(read_tls_td, this)) != nullptr) {
      _td_tls->detach();
    }
  } else {
    if ((_w_tls = new ev::io()) != nullptr) {
      _w_tls->set(_fd_tls, ev::READ);
      _w_tls->set<SOCKS5, &SOCKS5::read_tls_cb>(this);
      _w_tls->start();
    }
    if ((_w_tmo = new ev::timer()) != nullptr) {
      _w_tmo->set(tmo, 0);
      _w_tmo->set<SOCKS5, &SOCKS5::timeout_cb>(this);
      _w_tmo->start();
    }
  }
}

void SOCKS5::stop()
{
  _running = false;

  if (_multi) {
    if (_td_tls != nullptr) { delete _td_tls; _td_tls = nullptr; }
    if (_td_tgt != nullptr) { delete _td_tgt; _td_tgt = nullptr; }
  } else {
    if (_w_tls != nullptr) { delete _w_tls; _w_tls = nullptr; }
    if (_w_tgt != nullptr) { delete _w_tgt; _w_tgt = nullptr; }
    if (_w_tmo != nullptr) { delete _w_tmo; _w_tmo = nullptr; }
  }

  _done = true;
}

void SOCKS5::cleanup()
{
  stop();

  if (_ssl != nullptr && _tls != nullptr) { _tls->close(_ssl); _ssl = nullptr; }
  _target.close(_fd_tls);
  _target.close();

  log("Closing connection [%s:%u]", _ip_from.c_str(), _port_from);
}

short SOCKS5::stage_init(void* ptr, size_t len)
{
  short ns = STAGE_FINI;
  const char* buf = (const char*) ptr;

  if (len > 2 && buf[0] == SOCKS5_VER) {
    char rep[2] = { SOCKS5_VER, SOCKS5_METHOD_UNACCEPT };
    short i, num = buf[1];

    for (i = 0; i < num && (size_t) (i + 2) < len; i++) {
      if (buf[i + 2] == SOCKS5_METHOD_USRPASS) {
        rep[1] = SOCKS5_METHOD_USRPASS;
        ns = STAGE_AUTH;
        break;
      } else if (buf[i + 2] == SOCKS5_METHOD_NOAUTH && _nmpwd == nullptr) {
        rep[1] = SOCKS5_METHOD_NOAUTH;
        ns = STAGE_REQU;
        break;
      }
    }

    _tls->write(_ssl, rep, sizeof(rep));
  } else {
    char rep[2] = { SOCKS5_VER, SOCKS5_METHOD_UNACCEPT };
    _tls->write(_ssl, rep, sizeof(rep));
  }

  return ns;
}

short SOCKS5::stage_auth(void* ptr, size_t len)
{
  short ns = STAGE_FINI;
  const char* buf = (const char*) ptr;

  if (len > 3 && buf[0] == SOCKS5_AUTHVER) {
    char rep[2] = { SOCKS5_AUTHVER, SOCKS5_ERROR };

    if (_nmpwd != nullptr) {
      int nml = buf[1];
      string nm = string(buf + 2, nml);
      int pwdl = buf[2 + nml];
      string pwd = string(buf + 3 + nml, pwdl);
      auto lt = _nmpwd->find(nm);
      if (lt != _nmpwd->end() && pwd == lt->second) {
        rep[1] = SOCKS5_SUCCESS;
        ns = STAGE_REQU;
      }
    }

    _tls->write(_ssl, rep, sizeof(rep));
  } else {
    char rep[2] = { SOCKS5_AUTHVER, SOCKS5_ERROR };
    _tls->write(_ssl, rep, sizeof(rep));
  }

  return ns;
}

short SOCKS5::stage_requ(void* ptr, size_t len)
{
  short ns = STAGE_FINI;
  const char* buf = (const char*) ptr;

  if (len > 4 && buf[0] == SOCKS5_VER && buf[2] == '\0') {
    char cmd = buf[1];
    char aty = buf[3];
    char ips[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 2];
    char rep[MAX(STATUS_IPV4_LENGTH, STATUS_IPV6_LENGTH) + 2] = {0};

    if (cmd == SOCKS5_CMD_CONNECT) {
      bool okay = false;
      short rep_l = STATUS_IPV4_LENGTH;

      rep[0] = SOCKS5_VER;
      rep[1] = SOCKS5_REP_ERROR;

      if (aty == SOCKS5_ATYP_IPV4) {
        struct sockaddr_in sin;
        socklen_t sin_l = sizeof(sin);

        memset(&sin, 0, sizeof(sin));
        memcpy(&sin.sin_addr.s_addr, buf + 4, sizeof(sin.sin_addr.s_addr));
        memcpy(&sin.sin_port, buf + 8, sizeof(sin.sin_port));

        sin.sin_family = AF_INET;

        rep[3] = SOCKS5_ATYP_IPV4;

        if (inet_ntop(AF_INET, &sin.sin_addr.s_addr, ips, sizeof(ips)) != nullptr) {
          if (_target.connect((struct sockaddr*) &sin, sin_l) != -1) {
            ips[INET_ADDRSTRLEN] = '\0';
            log("[%s:%u] connected to [%s:%u] (ip4)", _ip_from.c_str(), _port_from, ips, ntohs(sin.sin_port));
            okay = true;    
          } else {
            log("[%s:%u] cannot connect to [%s:%u] (ip4)", _ip_from.c_str(), _port_from, ips, ntohs(sin.sin_port));
          }
        }
      } else if (aty == SOCKS5_ATYP_IPV6) {
        struct sockaddr_in6 sin6;
        socklen_t sin6_l = sizeof(sin6);

        memset(&sin6, 0, sizeof(sin6));
        memcpy(&sin6.sin6_addr, buf + 4, sizeof(sin6.sin6_addr));
        memcpy(&sin6.sin6_port, buf + 20, sizeof(sin6.sin6_port));

        sin6.sin6_family = AF_INET6;
      
        rep[3] = SOCKS5_ATYP_IPV6;
        
        if (inet_ntop(AF_INET6, &sin6.sin6_addr, ips, sizeof(ips)) != nullptr) {
          if (_target.connect((struct sockaddr*) &sin6, sin6_l) != -1) {
            ips[INET6_ADDRSTRLEN] = '\0';
            log("[%s]:(%u) connected to [%s]:(%u) (ip6)", _ip_from.c_str(), _port_from, ips, ntohs(sin6.sin6_port));
            rep_l = STATUS_IPV6_LENGTH;
            okay = true;
          } else {
            log("[%s]:(%u) cannot connect to [%s]:(%u) (ip6)", _ip_from.c_str(), _port_from, ips, ntohs(sin6.sin6_port));
          }
        }
      } else if (aty == SOCKS5_ATYP_DOMAINNAME) {
        rep[3] = SOCKS5_ATYP_IPV4;

        string hostip = string(buf + 5, buf[4]);
        short port = ntohs(*(short*) (buf + 5 + buf[4]));

        if (_target.connect(hostip.c_str(), port) != -1) {
          log("[%s:%u] connected to [%s:%u] (domain)", _ip_from.c_str(), _port_from, hostip.c_str(), port);
          okay = true;
        } else {
          log("[%s:%u] cannot connect to [%s:%u] (domain)", _ip_from.c_str(), _port_from, hostip.c_str(), port);
        }
      }

      if (okay && (_w_tgt = new ev::io()) != nullptr) {
        if (_multi) {
          if ((_td_tgt = new thread(read_tgt_td, this)) != nullptr) {
            _td_tgt->detach();
          }
        } else {
          _w_tgt->set(_target.socket(), ev::READ);
          _w_tgt->set<SOCKS5, &SOCKS5::read_tgt_cb>(this);
          _w_tgt->start();
        }
        rep[1] = SOCKS5_REP_SUCCESS;
        ns = STAGE_CONN;
        _tls->write(_ssl, rep, rep_l);
      } else {
        _target.close();
      }
    }
  }

  if (ns == STAGE_FINI) {
    char rep[STATUS_IPV4_LENGTH] = { SOCKS5_VER, SOCKS5_REP_ERROR, 0, SOCKS5_ATYP_IPV4, 0 };
    _tls->write(_ssl, rep, sizeof(rep));
  }

  return ns;
}

short SOCKS5::stage_conn(void* ptr, size_t len)
{
  short ns = STAGE_CONN;
  if (_target.send(ptr, len) <= 0) ns = STAGE_FINI;
  return ns;
}

short SOCKS5::stage_bind(void* ptr, size_t len)
{
  short ns = STAGE_FINI;
  return ns;
}

short SOCKS5::stage_udpp(void* ptr, size_t len)
{
  short ns = STAGE_FINI;
  return ns;
}

////

bool SOCKS5::read_tls()
{
  char buf[BUFSIZ * 4];
  int len;

  if ((len = _tls->read(_ssl, buf, sizeof(buf))) > 0) {
    switch (_stage) {
      case STAGE_INIT: _stage = stage_init(buf, len); break;
      case STAGE_AUTH: _stage = stage_auth(buf, len); break;
      case STAGE_REQU: _stage = stage_requ(buf, len); break;
      case STAGE_CONN: _stage = stage_conn(buf, len); break;
      case STAGE_BIND: _stage = stage_bind(buf, len); break;
      case STAGE_UDPP: _stage = stage_udpp(buf, len); break;
    }
    if (_stage == STAGE_FINI) return false;
    return true;
  } else {
    if (errno != 0) perror("read_tls");
    return false;
  }
}

bool SOCKS5::read_tgt()
{
  char buf[BUFSIZ * 4];
  ssize_t len;

  if ((len = _target.recv(buf, sizeof(buf))) > 0) {
    _tls->write(_ssl, buf, len);
    return true;
  } else {
    if (errno != 0) perror("read_tgt");
    return false;
  }
}

void SOCKS5::timeout()
{
  if (! _done) {
    char rep[INET_ADDRSTRLEN] = { SOCKS5_VER, SOCKS5_REP_TTLEXPI, 0, SOCKS5_ATYP_IPV4, 0 };
    _tls->write(_ssl, rep, sizeof(rep));
  }
}

void SOCKS5::read_tls_cb(ev::io& w, int revents)
{
  if (_w_tmo != nullptr) _w_tmo->stop();
  if (! read_tls()) cleanup();
  if (_w_tmo != nullptr) _w_tmo->again();
}

void SOCKS5::read_tgt_cb(ev::io& w, int revents)
{
  if (_w_tmo != nullptr) _w_tmo->stop();
  if (! read_tgt()) cleanup();
  if (_w_tmo != nullptr) _w_tmo->again();
}

void SOCKS5::timeout_cb(ev::timer& w, int revents)
{
  timeout();
  cleanup();
}

void SOCKS5::read_tls_td(SOCKS5* self)
{
  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(self->_fd_tls, &fds);

  struct timeval tmv = { .tv_sec = (time_t) self->_timeout, .tv_usec = 0 };

  int ret;

  while (self->_running) {
    if ((ret = select(self->_fd_tls + 1, &fds, nullptr, nullptr, &tmv)) > 0) {
      if (! self->read_tls()) break;
    } else {
      if (ret == 0) self->timeout();
      break;
    }
  }

  self->cleanup();
}

void SOCKS5::read_tgt_td(SOCKS5* self)
{
  fd_set fds;
  
  int fd = self->_target.socket();

  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  struct timeval tmv = { .tv_sec = (time_t) self->_timeout, .tv_usec = 0 };

  while (self->_running) {
    if (select(fd + 1, &fds, nullptr, nullptr, &tmv) > 0) {
      if (! self->read_tgt()) break;
    } else break;
  }

  self->cleanup();
}

////

bool SOCKS5::done()
{
  return _done;
}

void SOCKS5::nmpwd(map<string, string>* nmpwd)
{
  _nmpwd = nmpwd;
}

/*end*/
