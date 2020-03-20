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

using namespace std;
using namespace utils;

/////////////////////////////////////////////////

Server::Server() : 
  _running(false),
  _issrv(false),
  _norootfs(true),
  _ctimeout(DEF_CTIMEOUT),
  _stimeout(DEF_STIMEOUT),
  _loc_addrinfo(nullptr),
  _loop(nullptr), 
  _w_soc(nullptr),
  _w_loc(nullptr),
  _w_sig(nullptr),
  _td_cleanup(nullptr) {
  _nmpwd.clear();
  _lst_socks5.clear();
  _lst_client.clear();
  _lst_websv.clear();
}

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

  if (cfg.get("tls", "timeout", timeout)) {
    _ctimeout = atol(timeout.c_str());
  }
  
  if (cfg.get("main", "timeout", timeout)) {
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

  if (cfg.get("tls", "timeout", timeout)) {
    _ctimeout = atol(timeout.c_str());
  }

  if (cfg.get("main", "timeout", timeout)) {
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
    string rootfs;

    time_t tmo = _ctimeout;

    if (cfg.get("web", "timeout", timeout)) {
      tmo = atol(timeout.c_str());
    }

    _ctxwrapper.timeout() = tmo;

    if (cfg.get("web", "rootfs", rootfs)) {
      _ctxwrapper.opencpio(rootfs);
      _norootfs = false;
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

void Server::stop()
{
  _mutex_inprogress.lock();
  if (_issrv) stop_server();
  else stop_client();
  _mutex_inprogress.unlock();
}

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
  if (_loop  != nullptr) { delete _loop;  _loop  = nullptr; }
  if (_loc_addrinfo != nullptr) { _soc.resolve(nullptr, 0, &_loc_addrinfo); _loc_addrinfo = nullptr; }
  _running = false;
  if (_td_cleanup != nullptr) {
    _cv_cleanup.notify_one();
    _td_cleanup->join();
    delete _td_cleanup;
    _td_cleanup = nullptr;
  }
  _lst_client.clear();
  _loc.close(); // close socket
}

void Server::stop_server()
{
  if (_w_soc != nullptr) { delete _w_soc; _w_soc = nullptr; }
  if (_w_loc != nullptr) { delete _w_loc; _w_loc = nullptr; }
  if (_w_sig != nullptr) { delete _w_sig; _w_sig = nullptr; }
  if (_loop  != nullptr) { delete _loop;  _loop  = nullptr; }
  _running = false;
  if (_td_cleanup != nullptr) {
    _cv_cleanup.notify_one();
    _td_cleanup->join();
    delete _td_cleanup;
    _td_cleanup = nullptr;
  }
  _lst_socks5.clear();
  _ctxwrapper.closecpio();
  _loc.close();
  _soc.close();
}

////////////////////////////////////////////

bool Server::web_hdrinfo(const void* ptr, size_t len, string& cmd, string& path, string& ver)
{
  regex re("([A-Z]+)[\t ]+([^\r\n\t ]+*)[\t ]+([A-Z]+/[0-9]+\\.[0-9]+)");
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
  shared_ptr<SOCKS5> socks5 = make_shared<SOCKS5>();
  if (socks5) {
    socks5->start(this, fd, ip, port);
    _lst_socks5.push_back(socks5);
    log("[%s:%u] new connection", ip, port);
  } else error("soc_new_connection");
}

void Server::web_new_connection(int fd, const char* ip, int port)
{
  shared_ptr<WebSv> wsv = make_shared<WebSv>();
  if (wsv) {
    wsv->start(this, fd, ip, port);
    _lst_websv.push_back(wsv);
    log("[%s:%u] new connection to web service", ip, port);
  } else error("web_new_connection");
}

void Server::loc_new_connection(int fd, const char* ip, int port)
{
  shared_ptr<Client> cli = make_shared<Client>();
  if (cli) {
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
      _tls.write(ssl, (void*) DEF_CTX_SUCCESS, sizeof(DEF_CTX_SUCCESS) - 1);
      return true;
    } else {
      if (_norootfs) {
        if (cmd == "GET") {
          if (path == "/") {
            _tls.write(ssl, (void*) DEF_CTX_SUCCESS, sizeof(DEF_CTX_SUCCESS) - 1);
          } else {
            _tls.write(ssl, (void*) DEF_CTX_NOTFOUND, sizeof(DEF_CTX_NOTFOUND) - 1);
          }
        } else {
          _tls.write(ssl, (void*) DEF_CTX_BADREQUEST, sizeof(DEF_CTX_BADREQUEST) - 1);
        }
      } else {
        CPIOContent ctx;

        _ctxwrapper.request(cmd, path, ver, ctx);
        _tls.write(ssl, (void*) ctx.c_ptr, ctx.c_len);
      }
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

////////////////////////////////////////////

void Server::cleanup_td(Server* self)
{
  while (self->_running) {
    self->_mutex_inprogress.lock();
    unique_lock<mutex> lck(self->_mutex_cleanup);
    self->_cv_cleanup.wait_for(lck, chrono::seconds(self->_stimeout));
    if (self->_issrv) {
      self->_lst_socks5.remove_if([self](shared_ptr<SOCKS5>& socks5) {
        if (socks5->done() || (socks5->time() - ::time(nullptr)) > self->_stimeout) {
          return true;
        } else return false;
      });
      self->_lst_websv.remove_if([self](shared_ptr<WebSv>& websv) {
        if (websv->done() || (websv->time() - ::time(nullptr)) > self->_stimeout) {
          return true;
        } else return false;
      });
    } else {
      self->_lst_client.remove_if([self](shared_ptr<Client>& cli) {
        if (cli->done() || (cli->time() - ::time(nullptr)) > self->_stimeout) {
          return true;
        } else return false;
      });
    }
//    log("thread [%x]: _lst_socks5.size():%u, _lst_client.size():%u, _lst_websv.size():%u", ::time(nullptr), self->_lst_socks5.size(), self->_lst_client.size(), self->_lst_websv.size());
    self->_mutex_inprogress.unlock();
  }
}

/*end*/
