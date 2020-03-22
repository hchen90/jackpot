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
#include "config.h"
#include "socks5.h"
#include "server.h"
#include "utils.h"

using namespace std;
using namespace utils;

SOCKS5::SOCKS5()
: _fd_tls(-1),
  _port_from(0),
  _running(false),
  _iswebsv(false),
  _stage(STAGE_INIT),
  _timeout(DEF_CTIMEOUT),
  _nmpwd(nullptr),
  _tls(nullptr),
  _ssl(nullptr),
  _td_socks5(nullptr),
  _cv_cleanup(nullptr) {
  _ip_from.clear();
}

SOCKS5::~SOCKS5()
{ 
  if (! _iswebsv) {
    stop();
    if (_ssl != nullptr && _tls != nullptr) { _tls->close(_ssl); _ssl = nullptr; }
    _target.close(_fd_tls);
    _target.close();
    log("[%s:%u] closing connection", _ip_from.c_str(), _port_from);
  }
}
void SOCKS5::start(Server* srv, int fd, const string& ip_from, int port_from)
{
  if (! _running && (_td_socks5 = new thread(socks5_td, this, srv, fd, ip_from, port_from)) != nullptr) {
    _td_socks5->detach();
  }
}

void SOCKS5::stop()
{
  if (_running && ! _iswebsv) {
    _done = true;
    _running = false;
    if (_cv_cleanup != nullptr) {
      _cv_cleanup->notify_one();
    }
    if (_td_socks5 != nullptr) {
      delete _td_socks5;
      _td_socks5 = nullptr;
    }
  }
}

bool SOCKS5::done()
{
  return _done;
}

time_t SOCKS5::time()
{
  return _latest;
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
  char buf[BUFSIZE];
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
  char buf[BUFSIZE];
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
        rep[1] = SOCKS5_REP_REFUSED;
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
            rep[1] = SOCKS5_REP_SUCCESS;
            ips[INET_ADDRSTRLEN] = '\0';
            log("[%s:%u] connected to [%s:%u] (ip4)", _ip_from.c_str(), _port_from, ips, ntohs(sin.sin_port));
            ns = STAGE_CONN;
          } else {
            rep[1] = SOCKS5_REP_HOSTUNREACH;
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
          log("[%s:/%u] try to reach [%s:/%u] (ip6)", _ip_from.c_str(), _port_from, ips, ntohs(sin6.sin6_port));
          if (_target.connect((struct sockaddr*) &sin6, sin6_l) != -1) {
            rep[1] = SOCKS5_REP_SUCCESS;
            ips[INET6_ADDRSTRLEN] = '\0';
            log("[%s:/%u] connected to [%s:/%u] (ip6)", _ip_from.c_str(), _port_from, ips, ntohs(sin6.sin6_port));
            rep_l = STATUS_IPV6_LENGTH;
            ns = STAGE_CONN;
          } else {
            rep[1] = SOCKS5_REP_HOSTUNREACH;
            log("[%s:/%u] cannot connect to [%s:/%u] (ip6)", _ip_from.c_str(), _port_from, ips, ntohs(sin6.sin6_port));
          }
        }
      } else if (aty == SOCKS5_ATYP_DOMAINNAME) {
        rep[3] = SOCKS5_ATYP_IPV4;

        string hostip = string(buf + 5, buf[4]);
        short port = ntohs(*(short*) (buf + 5 + buf[4]));

        log("[%s:%u] try to reach [%s:%u] (domain)", _ip_from.c_str(), _port_from, hostip.c_str(), port);
        if (_target.connect(hostip.c_str(), port) != -1) {
          rep[1] = SOCKS5_REP_SUCCESS;
          log("[%s:%u] connected to [%s:%u] (domain)", _ip_from.c_str(), _port_from, hostip.c_str(), port);
          ns = STAGE_CONN;
        } else {
          rep[1] = SOCKS5_REP_HOSTUNREACH;
          log("[%s:%u] cannot connect to [%s:%u] (domain)", _ip_from.c_str(), _port_from, hostip.c_str(), port);
        }
      }

      _tls->write(_ssl, rep, rep_l);
    }
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

  bool endloop = false;

  while (_running && ! endloop) {
    memcpy(&fds, &rfds, sizeof(fds));

    struct timeval tmv = { .tv_sec = _timeout, .tv_usec = 0 };

    switch (select(MAX(fd_tgt, _fd_tls) + 1, &fds, nullptr, nullptr, &tmv)) {
      case 0:
        timeout();
        endloop = true;
        log("[%s:%u] socks5 timeout elapsed (%u)", _ip_from.c_str(), _port_from, _timeout);
        break;
      case -1:
        if (errno == EINTR) continue;
        else error("stage_conn");
        endloop = true;
        break;
    }

    if (! endloop) {
      _latest = ::time(nullptr);
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

void SOCKS5::socks5_td(SOCKS5* self, Server* srv, int fd, const string& ip_from, int port_from)
{
  if (self->init(srv, fd, ip_from, port_from)) {
    if (self->_iswebsv) self->WebSrv::transfer();
    else self->transfer();
  } else {
    self->stop();
  }
}

bool SOCKS5::init(Server* srv, int fd, const string& ip_from, int port_from)
{
  _running = true;

  SSL* ssl = srv->_tls.ssl(ip_from, port_from);

  _fd_tls = fd;
  _ip_from = ip_from;
  _port_from = port_from;
  _cv_cleanup = &srv->_cv_cleanup;

  if (ssl != nullptr && srv->_tls.fd(ssl, fd) > 0 && srv->_tls.accept(ssl) > 0) {
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    struct timeval tmv = { .tv_sec = srv->_ctimeout, .tv_usec = 0 };

    switch (select(fd + 1, &fds, nullptr, nullptr, &tmv)) {
      case -1:
      case 0:
        break;
      default:
        if (srv->soc_accept(ssl)) {
          _tls = &srv->_tls;
          _ssl = ssl;
          _timeout = srv->_ctimeout;
          if (! srv->_nmpwd.empty()) {
            _nmpwd = &srv->_nmpwd;
          }
          return true;
        } else {
          if (WebSrv::init(srv, fd, ip_from, port_from, ssl)) {
            return _iswebsv = true;
          }
        }
        break;
    }
  }

  if (errno != 0 && errno != EINPROGRESS) error("init()");
  if (ssl != nullptr) srv->_tls.close(ssl);

  stop();

  return false;
}

void SOCKS5::transfer()
{
  char buf[MAX(BUFSIZ, 1024)];
  ssize_t len;

  while ((len = _tls->read(_ssl, buf, sizeof(buf))) > 0) {
    switch (_stage) {
      case STAGE_INIT: _stage = stage_init(buf, len); break;
      case STAGE_AUTH: _stage = stage_auth(buf, len); break;
      case STAGE_REQU: _stage = stage_requ(buf, len); break;
    }
    if (_stage == STAGE_CONN) {
      _stage = stage_conn();
    } else if (_stage == STAGE_BIND) {
      _stage = stage_bind();
    } else if (_stage == STAGE_UDPP) {
      _stage = stage_udpp();
    }
    if (_stage == STAGE_FINI) {
      break;
    }
    _latest = ::time(nullptr);
  }

  stop();
}

/*end*/
