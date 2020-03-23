/* ***
 * @ $client.cpp
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
#include "config.h"
#include "server.h"
#include "utils.h"

using namespace std;
using namespace utils;

/////////////////////////////////////////////////

Client::Client()
: _ssl(nullptr),
  _fd_cli(-1),
  _done(false),
  _running(false),
  _latest(0),
  _port_from(0),
  _td_trf(nullptr),
  _server(nullptr) {
  _ip_from.clear();
}

Client::~Client()
{ 
  stop();
  if (_ssl != nullptr && _server != nullptr) { _server->_tls.close(_ssl); _ssl = nullptr; }
  _host.close(_fd_cli);
  _host.close();
  log("[%s:%u] closing connection", _ip_from.c_str(), _port_from);
}

void Client::start(Server* srv, int fd, const string& ip_from, int port_from)
{
  if (! _running && (_td_trf = new thread(client_td, this, srv, fd, ip_from, port_from)) != nullptr) {
    _td_trf->detach();
  }
}

void Client::stop()
{
  if (_running) {
    _done = true;
    _running = false;
    if (_server != nullptr) {
      unique_lock<mutex> lck(_server->_mutex_cleanup);
      _server->_cv_cleanup.notify_one();
    }
    if (_td_trf != nullptr) {
      delete _td_trf;
      _td_trf = nullptr;
    }
  }
}

bool Client::done()
{
  return _done;
}

time_t Client::time()
{
  return _latest;
}

bool Client::read_cli()
{
  bool okay = true;
  char buf[BUFSIZE];
  ssize_t len, sent = 0;

  if ((len = _host.recv(_fd_cli, buf, sizeof(buf))) > 0) {
    while (len > sent) {
      int num = _server->_tls.write(_ssl, buf, len);
      if (num > 0) sent += num;
      else { okay = false; break; }
    }
  } else okay = false;

  if (! okay && errno != 0) error("read_cli");
  return okay;
}

bool Client::read_tls()
{
  bool okay = true;
  char buf[BUFSIZE];
  ssize_t len, sent = 0;

  if ((len = _server->_tls.read(_ssl, buf, sizeof(buf))) > 0) {
    while (len > sent) {
      int num = _host.send(_fd_cli, buf, len);
      if (num > 0) sent += num;
      else { okay = false; break; }
    }
  } else okay = false;

  if (! okay && errno != 0) error("read_tls");
  return okay;
}

void Client::client_td(Client* self, Server* srv, int fd, const string& ip_from, int port_from)
{
  if (self->init(srv, fd, ip_from, port_from)) {
    self->transfer();
  } else {
    self->stop();
  }
}

bool Client::init(Server* srv, int fd, const string& ip_from, int port_from)
{
  if (srv == nullptr) return false;

  _running = true;

  SSL* ssl = srv->_tls.ssl(ip_from, port_from);

  _fd_cli = fd;
  _ip_from = ip_from;
  _port_from = port_from;
  _server = srv;

  if (ssl != nullptr && _host.connect(srv->_soc.gethostip().c_str(), srv->_soc.getport()) != -1 && srv->_tls.fd(ssl, _host.socket()) > 0 && srv->_tls.connect(ssl) > 0) {
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(_host.socket(), &fds);

    struct timeval tmv = { .tv_sec = srv->_ctimeout, .tv_usec = 0 };

    switch (select(_host.socket() + 1, nullptr, &fds, nullptr, &tmv)) {
      case -1:
      case 0:
        break;
      default:
        if (srv->loc_accept(ssl)) {
          _ssl = ssl;
          return true;
        }
        break;
    }
  }

  if (errno != 0 && errno != EINPROGRESS) error("init()");
  if (ssl != nullptr) srv->_tls.close(ssl);

  stop();

  return false;
}

void Client::transfer()
{
  fd_set rfds, fds;

  int fd = _host.socket();

  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);
  FD_SET(_fd_cli, &rfds);

  while (_running) {
    memcpy(&fds, &rfds, sizeof(fds));

    struct timeval tmv = { .tv_sec = _server->_ctimeout, .tv_usec = 0 };
    
    if (select(MAX(fd, _fd_cli) + 1, &fds, nullptr, nullptr, &tmv) > 0) {
      _latest = ::time(nullptr);
      if (FD_ISSET(fd, &fds)) { // from _host
        if (! read_tls()) break;
      } else { // from client
        if (! read_cli()) break;
      }
    } else {
      if (errno == EINTR) continue;
      break;
    }
  }

  stop();
}

/*end*/
