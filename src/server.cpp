/* ***
 * @ $server.cpp
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

#include <unistd.h>
#include <execinfo.h>

#include "config.h"
#include "server.h"
#include "utils.h"

#define DEF_CTX_BADREQUEST "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><head><title>Bad Request</title></head><body><h1>400 Bad Request</h1><p>Unknown request</p></body></html>"
#define DEF_CTX_NOTFOUND "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>File cannot be found</p></body></html>"
#define DEF_CTX_SUCCESS "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><head><title>Welcome</title><h1>Welcome</h1><p>This page is only for test</p></head></html>"

using namespace std;
using namespace utils;

/////////////////////////////////////////////////

Client::Client() : _tls(nullptr), _ssl(nullptr), _fd_cli(-1), _done(false), _running(false), _timeout(DEF_CTIMEOUT), _latest(0), _port_from(0), _td_trf(nullptr), _cv_cleanup(nullptr) {}

Client::~Client()
{ 
  stop();
  if (_ssl != nullptr && _tls != nullptr) { _tls->close(_ssl); _ssl = nullptr; }
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
    if (_cv_cleanup) {
      _cv_cleanup->notify_one();
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
      int num = _tls->write(_ssl, buf, len);
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

  if ((len = _tls->read(_ssl, buf, sizeof(buf))) > 0) {
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
  _running = true;

  SSL* ssl = srv->_tls.ssl(ip_from, port_from);

  _fd_cli = fd;
  _ip_from = ip_from;
  _port_from = port_from;
  _cv_cleanup = &srv->_cv_cleanup;

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
          _tls = &srv->_tls;
          _ssl = ssl;
          _timeout = srv->_ctimeout;
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

  struct timeval tmv = { .tv_sec = _timeout, .tv_usec = 0 };

  while (_running) {
    memcpy(&fds, &rfds, sizeof(fds));

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

/////////////////////////////////////////////////

Server::Server() : 
  _running(false),
  _issrv(false),
  _ctimeout(DEF_CTIMEOUT),
  _stimeout(DEF_STIMEOUT),
  _loc_addrinfo(nullptr),
  _loop(nullptr), 
  _w_soc(nullptr),
  _w_loc(nullptr),
  _w_sig(nullptr),
  _w_tmo(nullptr),
  _td_cleanup(nullptr) {}

Server::~Server()
{
  if (! access(_pidfile.c_str(), F_OK)) remove(_pidfile.c_str());
}

bool Server::client(Conf& cfg)
{
  if (! _tls.init()) { _tls.error(); return false; }

  string pidfl, logfl;
  
  if (cfg.get("main", "pidfile", pidfl) && ! pidfile(pidfl)) return false;

  if (! cfg.get("main", "serial", _serial)) return false;

  string timeout;

  if (cfg.get("main", "ctimeout", timeout)) {
    _ctimeout = atol(timeout.c_str());
  }
  
  if (cfg.get("main", "stimeout", timeout)) {
    _stimeout = atol(timeout.c_str());
  }

  string ip_tls, port_tls;

  if (! cfg.get("tls", "ip", ip_tls)) return false;
  if (! cfg.get("tls", "port", port_tls)) port_tls = "443";

  string ip_local, port_local;

  if (! cfg.get("local", "ip", ip_local)) ip_local = "0.0.0.0";
  if (! cfg.get("local", "port", port_local)) port_local = "1080";

  int port_tls_n = atoi(port_tls.c_str()), port_local_n = atoi(port_local.c_str());

  if (port_tls_n <= 0) port_tls_n = 443;
  if (port_local_n <= 0) port_local_n = 1080;

  if (_soc.resolve(ip_tls.c_str(), port_tls_n, &_loc_addrinfo) != -1 && \
      _loc.bind(ip_local.c_str(), port_local_n) != -1 && _loc.listen() != -1) {
    _running = true;
    return true;
  } else {
    error("client()");
    return false;
  }
}

bool Server::server(Conf& cfg)
{
  string key, cert;

  if (cfg.get("main", "private_key", key) && cfg.get("main", "certificate", cert)) {
    if (! _tls.init(key, cert)) { _tls.error(); return false; }
  }

  string pidfl;
  
  if (cfg.get("main", "pidfile", pidfl) && ! pidfile(pidfl)) return false;

  if (! cfg.get("main", "serial", _serial)) return false;

  string timeout;

  if (cfg.get("main", "ctimeout", timeout)) {
    _ctimeout = atol(timeout.c_str());
  }

  if (cfg.get("main", "stimeout", timeout)) {
    _stimeout = atol(timeout.c_str());
  }

  string ip_tls, port_tls;

  if (! cfg.get("tls", "ip", ip_tls)) ip_tls = "0.0.0.0";
  if (! cfg.get("tls", "port", port_tls)) port_tls = "443";
  
  string ip_web, port_web, page;

  cfg.get("web", "page", page);

  if (! cfg.get("web", "ip", ip_web)) ip_web = "0.0.0.0";
  if (! cfg.get("web", "port", port_web)) port_web = "80";

  int port_tls_n = atoi(port_tls.c_str()), port_web_n = atoi(port_web.c_str());

  if (port_tls_n <= 0) port_tls_n = 443;
  if (port_web_n <= 0) port_web_n = 80;

  if (_soc.bind(ip_tls.c_str(), port_tls_n) != -1 && _soc.listen() != -1 && \
      _loc.bind(ip_web.c_str(), port_web_n) != -1 && _loc.listen() != -1) {
    if (! web_initpage(page)) {
      _200_ctx = DEF_CTX_SUCCESS;
      _400_ctx = DEF_CTX_BADREQUEST;
      _404_ctx = DEF_CTX_NOTFOUND;
    }
    socks5_initnmpwd(cfg);
    _running = true;
    _issrv = true;
    return true;
  } else {
    error("server()");
    return false;
  }
}

bool Server::pidfile(const string& pidfl)
{
  if (! access(pidfl.c_str(), F_OK)) return false;

  _pidfile = pidfl;

  ofstream fout(pidfl.c_str(), ios::out);

  if (fout.good()) {
    fout << (uint32_t) getpid() << endl;
    fout.close();
    return true;
  } else return false;
}

void Server::start() { if (_issrv) start_server(); else start_client(); }

void Server::stop() { if (_issrv) stop_server(); else stop_client(); }

void Server::start_client()
{
  if (! _running) return;

  if ((_loop = new ev::default_loop()) != nullptr) {
    if ((_w_loc = new ev::io()) != nullptr) {
      _w_loc->set(_loc.socket(), ev::READ);
      _w_loc->set<Server, &Server::loc_accept_cb>(this);
      _w_loc->start();
    }
    if ((_w_sig = new ev::sig()) != nullptr) {
      _w_sig->set(SIGINT);
      _w_sig->set<Server, &Server::signal_cb>(this);
      _w_sig->start();
    }
    if ((_w_tmo = new ev::timer()) != nullptr) {
      _w_tmo->set(_stimeout, _stimeout);
      _w_tmo->set<Server, &Server::timeout_cb>(this);
      _w_tmo->start();
    }
    _td_cleanup = new thread(cleanup_td, this);
    log("SOCKS5 server is listening on [%s:%u]", _loc.gethostip().c_str(), _loc.getport());
    _loop->run();
  } else _running = false;
}

void Server::start_server()
{
  if (! _running) return;

  if ((_loop = new ev::default_loop()) != nullptr) {
    if ((_w_soc = new ev::io()) != nullptr) {
      _w_soc->set(_soc.socket(), ev::READ);
      _w_soc->set<Server, &Server::soc_accept_cb>(this);
      _w_soc->start();
    }
    if ((_w_loc = new ev::io()) != nullptr) {
      _w_loc->set(_loc.socket(), ev::READ);
      _w_loc->set<Server, &Server::web_accept_cb>(this);
      _w_loc->start();
    }
    if ((_w_sig = new ev::sig()) != nullptr) {
      _w_sig->set(SIGINT);
      _w_sig->set<Server, &Server::signal_cb>(this);
      _w_sig->start();
    }
    if ((_w_tmo = new ev::timer()) != nullptr) {
      _w_tmo->set(_stimeout, _stimeout);
      _w_tmo->set<Server, &Server::timeout_cb>(this);
      _w_tmo->start();
    }
    _td_cleanup = new thread(cleanup_td, this);
    log("Proxy server is listening on [%s:%u]", _soc.gethostip().c_str(), _soc.getport());
    log("Web server is listening on [%s:%u]", _loc.gethostip().c_str(), _loc.getport());
    _loop->run();
  } else _running = false;
}

void Server::stop_client()
{
  if (_w_loc != nullptr) { delete _w_loc; _w_loc = nullptr; }
  if (_w_sig != nullptr) { delete _w_sig; _w_sig = nullptr; }
  if (_w_tmo != nullptr) { delete _w_tmo; _w_tmo = nullptr; }
  if (_loop  != nullptr) { delete _loop;  _loop  = nullptr; }
  if (_loc_addrinfo != nullptr) { _soc.resolve(nullptr, 0, &_loc_addrinfo); _loc_addrinfo = nullptr; }
  _running = false;
  if (_td_cleanup != nullptr) {
    _cv_cleanup.notify_one();
    _td_cleanup->join();
    delete _td_cleanup;
    _td_cleanup = nullptr;
  }
  for (auto& it : _lst_client) delete it;
  _lst_client.clear();
  _loc.close(); // close socket
}

void Server::stop_server()
{
  if (_w_soc != nullptr) { delete _w_soc; _w_soc = nullptr; }
  if (_w_loc != nullptr) { delete _w_loc; _w_loc = nullptr; }
  if (_w_sig != nullptr) { delete _w_sig; _w_sig = nullptr; }
  if (_w_tmo != nullptr) { delete _w_tmo; _w_tmo = nullptr; }
  if (_loop  != nullptr) { delete _loop;  _loop  = nullptr; }
  _running = false;
  if (_td_cleanup != nullptr) {
    _cv_cleanup.notify_one();
    _td_cleanup->join();
    delete _td_cleanup;
    _td_cleanup = nullptr;
  }
  for (auto& it : _lst_socks5) delete it;
  _lst_socks5.clear();
  _loc.close();
  _soc.close();
}

////////////////////////////////////////////

bool Server::web_hdrinfo(const void* ptr, size_t len, string& cmd, string& path, string& ver)
{
  regex re("([A-Z]+)[\t ]+(/[A-Za-z_0-9]+*)[\t ]+([A-Z]+/[0-9]+\\.[0-9]+)");
  smatch sm;
  string str((const char*) ptr, MIN(len, 64));

  if (regex_search(str, sm, re) && sm.size() == 4) {
    cmd = sm[1];
    path = sm[2];
    ver = sm[3];
    return true;
  }

  return false;
}

bool Server::web_initpage(const string& page)
{
  if (page.empty()) return false;

  ifstream fin(page.c_str(), ios::in);

  if (fin.good()) {
    string line, buf;
    int nc = 0;

    while (true) {
      if (getline(fin, line)) {
        if (nc == 1)
          buf += line;
        else 
          buf += line + "\r\n";
        if (line.empty()) nc++;
        if (nc == 2) {
          if (! buf.compare(9, 3, "200")) _200_ctx = buf;
          else if (! buf.compare(9, 3, "400")) _400_ctx = buf;
          else if (! buf.compare(9, 3, "404")) _404_ctx = buf;
          buf.clear();
          nc = 0;
        }
      } else {
        if (! buf.empty()) {
          if (! buf.compare(9, 3, "200")) _200_ctx = buf;
          else if (! buf.compare(9, 3, "400")) _400_ctx = buf;
          else if (! buf.compare(9, 3, "404")) _404_ctx = buf;
        }
        break;
      }
    }

    fin.close();
    return true;
  } else {
    error(page.c_str());
    return false;
  }
}

void Server::socks5_initnmpwd(Conf& cfg)
{
  char key[64];
  int count = 0;
  bool okay = true;

  while (okay) {
    if (snprintf(key, sizeof(key), "user%u", count++) > 0) {
      string value;
      if (cfg.get("user", key, value)) {
        vector<string> res;
        if (token(value, ":", res) && res.size() > 1) {
          _nmpwd.insert(make_pair(res[0], res[1]));
        }
      } else okay = false;
    } else okay = false;
  }
}

////////////////////////////////////////////

void Server::soc_new_connection(int fd, const char* ip, int port)
{
  SOCKS5* socks5 = new SOCKS5();
  if (socks5 != nullptr) {
    socks5->start(this, fd, ip, port);
    _lst_socks5.push_back(socks5);
    log("[%s:%u] new connection", ip, port);
  } else error("soc_new_connection");
}

void Server::web_new_connection(int fd, const char* ip, int port)
{
  char buf[BUFSIZ]; size_t len;
  bool okay = false, done = false;

  while ((len = _loc.recv(fd, buf, sizeof(buf))) > 0) {
    if (! done) {
      string cmd, path, ver;

      if (web_hdrinfo(buf, len, cmd, path, ver) && cmd == "GET") {
        if (path == "/") {
          if (_loc.send(fd, _200_ctx.data(), _200_ctx.size()) > 0)
            okay = true;
        } else {
          if (_loc.send(fd, _404_ctx.data(), _404_ctx.size()) > 0)
            okay = true;
        }
      }
      done = true;
    }
    if (len == sizeof(buf)) continue;
    else break;
  }

  if (! okay) _loc.send(fd, _400_ctx.data(), _400_ctx.size());

  _loc.close(fd);

  log("[%s:%u] new connection to web service", ip, port);
}

void Server::loc_new_connection(int fd, const char* ip, int port)
{
  Client* cli = new Client();

  if (cli != nullptr) {
    cli->start(this, fd, ip, port);
    _lst_client.push_back(cli);
    log("[%s:%u] new connection", ip, port);
  } else error("loc_new_connection");
}

////

bool Server::loc_accept(SSL* ssl)
{
  string req = "GET /";

  req += _serial + " HTTP/1.1\r\nHost: ";
  req += _soc.gethostip() + ":";

  char buf[MAX(BUFSIZ, 1024)];

  snprintf(buf, sizeof(buf), "%u\r\n", _soc.getport());

  req += buf;
  req += "Content-Type: text/html\r\nConnection: keep-alive\r\nContent-Language: en\r\n\r\n";

  if (_tls.write(ssl, (void*) req.data(), req.size()) <= 0) {
    _tls.error(ssl);
    return false;
  }

  size_t len;

  if ((len = _tls.read(ssl, buf, sizeof(buf))) <= 0) return false;

  if (len >= sizeof(buf)) log("Response is too long");

  string str = string((const char*) buf, MIN(len, 16));

  if (! str.compare(9, 3, "200")) return true;

  return false;
}

bool Server::soc_accept(SSL* ssl)
{
  char buf[MAX(BUFSIZ, 1024)];
  size_t len;

  if ((len = _tls.read(ssl, buf, sizeof(buf))) <= 0) return false;

  if (len >= sizeof(buf)) log("Request is too long");

  string cmd, path, ver;

  if (web_hdrinfo(buf, len, cmd, path, ver)) {
    string ssr;
    if (_serial.c_str()[0] == '/')
      ssr = _serial;
    else {
      ssr = "/"; ssr += _serial;
    }
    if (cmd == "GET" && path == ssr) {
      _tls.write(ssl, (void*) _200_ctx.data(), _200_ctx.size());
      return true;
    } else if (cmd == "GET" && path == "/") {
      _tls.write(ssl, (void*) _200_ctx.data(), _200_ctx.size());
    } else {
      _tls.write(ssl, (void*) _404_ctx.data(), _404_ctx.size());
    }
  }
  
  return false;
}

////////////////////////////////////////////

void Server::soc_accept_cb(ev::io& w, int revents)
{
  w.stop();

  char ip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 2];
  int port, fd = _soc.accept(ip, port);

  if (fd != -1) {
    soc_new_connection(fd, ip, port);
  } else error("soc_accept");
  
  w.start();
}

void Server::web_accept_cb(ev::io& w, int revents)
{
  w.stop();

  char ip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 2];
  int port, fd = _loc.accept(ip, port);

  if (fd != -1) {
    web_new_connection(fd, ip, port);
  } else error("web_accept");

  w.start();
}

void Server::loc_accept_cb(ev::io& w, int revents)
{
  w.stop();

  char ip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 2];
  int port, fd = _loc.accept(ip, port);

  if (fd != -1) {
    loc_new_connection(fd, ip, port);
  } else error("loc_accept");
  
  w.start();
}

void Server::signal_cb(ev::sig& w, int revents)
{
  w.stop();

  stop();
  log("Exitting...");
  exit(EXIT_SUCCESS);

  w.start();
}

void Server::timeout_cb(ev::timer& w, int revents)
{
  w.stop();

  _cv_cleanup.notify_one();

  w.again();
}

////////////////////////////////////////////

void Server::cleanup_td(Server* self)
{
  while (self->_running) {
    unique_lock<mutex> lck(self->_mutex_cleanup);
    self->_cv_cleanup.wait(lck);
    if (self->_issrv) {
      self->_lst_socks5.remove_if([self](SOCKS5*& socks5) {
        if (socks5->done() || (socks5->time() - ::time(nullptr)) > self->_ctimeout * 2) {
          delete socks5;
          return true;
        } else return false;
      });
    } else {
      self->_lst_client.remove_if([self](Client*& cli) {
        if (cli->done() || (cli->time() - ::time(nullptr)) > self->_ctimeout * 2) {
          delete cli;
          return true;
        } else return false;
      });
    }
    //log("thread [%x]: _lst_socks5.size():%u, _lst_client.size():%u", ::time(nullptr), self->_lst_socks5.size(), self->_lst_client.size());
  }
}

/*end*/
