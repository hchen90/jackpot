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
using namespace utils;

SOCKS5::SOCKS5() : _fd_tls(-1), _port_from(0), _done(false), _running(false), _stage(STAGE_INIT), _timeout(DEF_TIMEOUT), _nmpwd(nullptr), _tls(nullptr), _ssl(nullptr), _td_socks5(nullptr), _cv_cleanup(nullptr) {}

SOCKS5::~SOCKS5() { cleanup(); }

bool SOCKS5::init(TLS* tls, SSL* ssl, int fd, const string& ip_from, int port_from)
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

void SOCKS5::start(condition_variable* cv, time_t tmo)
{
  if ((_td_socks5 = new thread(socks5_td, this)) != nullptr) {
    _td_socks5->detach();
    _cv_cleanup = cv;
    _timeout = tmo;
    _running = true;
  }
}

void SOCKS5::stop()
{
  if (_running) {
    if (_td_socks5 != nullptr) {
      delete _td_socks5;
      _td_socks5 = nullptr;
    }
    _done = true;
    _running =false;
  }
}

void SOCKS5::cleanup()
{
  stop();

  if (_ssl != nullptr && _tls != nullptr) { _tls->close(_ssl); _ssl = nullptr; }
  _target.close(_fd_tls);
  _target.close();

  log("Closing connection [%s:%u]", _ip_from.c_str(), _port_from);

  if (_cv_cleanup != nullptr) {
    _cv_cleanup->notify_one();
  }
}

void SOCKS5::timeout()
{
  if (_running) {
    char rep[STATUS_IPV4_LENGTH] = { SOCKS5_VER, SOCKS5_REP_TTLEXPI, 0, SOCKS5_ATYP_IPV4, 0,0,0,0, 0,0 };
    _tls->write(_ssl, rep, sizeof(rep));
  }
}

bool SOCKS5::read_tls()
{
  bool okay = true;
  char buf[BUFSIZ];
  ssize_t len, sent = 0;

  if ((len = _tls->read(_ssl, buf, sizeof(buf))) > 0) {
    while (len > sent) {
      int num = _target.send(buf, len);
      if (num > 0) sent += num;
      else { okay = false; break; }
    }
  } else okay = false;

  if (! okay && errno != 0) error("read_tls");
  return okay;
}

bool SOCKS5::read_tgt()
{
  bool okay = true;
  char buf[BUFSIZ];
  ssize_t len, sent = 0;

  if ((len = _target.recv(buf, sizeof(buf))) > 0) {
    while (len > sent) {
      int num = _tls->write(_ssl, buf, len);
      if (num > 0) sent += num;
      else { okay = false; break; }
    }
  } else okay = false;

  if (! okay && errno != 0) error("read_tls");
  return okay;
}

////

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
      } else if (buf[i + 2] == SOCKS5_METHOD_NOAUTH && (_nmpwd == nullptr || (_nmpwd != nullptr && _nmpwd->empty()))) {
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
      } else {
        log("Authentication failed");
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
          log("[%s:%u] try to reach [%s:%u] (ip4)", _ip_from.c_str(), _port_from, ips, ntohs(sin.sin_port));
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
          log("[%s]:(%u) try to reach [%s]:(%u) (ip6)", _ip_from.c_str(), _port_from, ips, ntohs(sin6.sin6_port));
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

        log("[%s:%u] try to reach [%s:%u] (domain)", _ip_from.c_str(), _port_from, hostip.c_str(), port);
        if (_target.connect(hostip.c_str(), port) != -1) {
          log("[%s:%u] connected to [%s:%u] (domain)", _ip_from.c_str(), _port_from, hostip.c_str(), port);
          okay = true;
        } else {
          log("[%s:%u] cannot connect to [%s:%u] (domain)", _ip_from.c_str(), _port_from, hostip.c_str(), port);
        }
      }

      if (okay) {
        rep[1] = SOCKS5_REP_SUCCESS;
        ns = STAGE_CONN;
        _tls->write(_ssl, rep, rep_l);
      }
    }
  }

  if (ns == STAGE_FINI) {
    char rep[STATUS_IPV4_LENGTH] = { SOCKS5_VER, SOCKS5_REP_ERROR, 0, SOCKS5_ATYP_IPV4, 0 };
    _tls->write(_ssl, rep, sizeof(rep));
  }

  return ns;
}

short SOCKS5::stage_conn()
{
  int fd_tgt = _target.socket();

  fd_set rfds, fds;

  FD_ZERO(&rfds);
  FD_SET(_fd_tls, &rfds);
  FD_SET(fd_tgt, &rfds);

  struct timeval tmv = { .tv_sec = _timeout, .tv_usec = 0 };

  bool endloop = false;

  while (_running && ! endloop) {
    memcpy(&fds, &rfds, sizeof(fds));

    switch (select(MAX(fd_tgt, _fd_tls) + 1, &fds, nullptr, nullptr, &tmv)) {
      case 0: timeout(); endloop = true; break;
      case -1: if (errno == EINTR) continue; else error("stage_conn"); endloop = true; break;
    }

    if (! endloop) {
      if (FD_ISSET(fd_tgt, &fds)) {
        if (! read_tgt()) endloop = true;
      } else {
        if (! read_tls()) endloop = true;
      }
    }
  }

  return STAGE_FINI;
}

short SOCKS5::stage_bind()
{
  log("not spport BIND yet :(");
  return STAGE_FINI;
}

short SOCKS5::stage_udpp()
{
  log("not support UDP yet :(");
  return STAGE_FINI;
}

////

void SOCKS5::socks5_td(SOCKS5* self)
{
  char buf[MAX(BUFSIZ, 1024)];
  ssize_t len;

  while ((len = self->_tls->read(self->_ssl, buf, sizeof(buf))) > 0) {
    switch (self->_stage) {
      case STAGE_INIT: self->_stage = self->stage_init(buf, len); break;
      case STAGE_AUTH: self->_stage = self->stage_auth(buf, len); break;
      case STAGE_REQU: self->_stage = self->stage_requ(buf, len); break;
    }
    if (self->_stage == STAGE_CONN) {
      self->_stage = self->stage_conn();
    } else if (self->_stage == STAGE_BIND) {
      self->_stage = self->stage_bind();
    } else if (self->_stage == STAGE_UDPP) {
      self->_stage = self->stage_udpp();
    }
    if (self->_stage == STAGE_FINI) {
      break;
    }
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
