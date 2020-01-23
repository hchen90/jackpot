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
#include <cstring>
#include <fstream>

#include <unistd.h>

#include "config.h"
#include "buffer.h"
#include "server.h"
#include "utils.h"

#define DEF_CTX_BADREQUEST "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><head><title>Bad Request</title></head><body><h1>400 Bad Request</h1><p>Unknown request</p></body></html>"
#define DEF_CTX_NOTFOUND "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>File cannot be found</p></body></html>"
#define DEF_CTX_SUCCESS "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><head><title>Welcome</title><h1>Welcome</h1><p>This page is only for test</p></head></html>"

#define DEF_TIMEOUT 20

using namespace std;

/////////////////////////////////////////////////

Client::Client() : _tls(nullptr), _ssl(nullptr), _fd_cli(-1), _w_cli(nullptr), _w_tls(nullptr), _done(false), _port_from(0) {}

Client::~Client() { cleanup(); }

bool Client::init(const string& hostip, int port, int fd, TLS* tls, SSL* ssl, const string& ip_from, int port_from)
{
  if (_host.socket(0x11) != -1 && _host.connect(hostip.c_str(), port) != -1 && tls->fd(ssl, _host.socket()) > 0 && tls->connect(ssl) > 0) {
    _fd_cli = fd;
    _tls = tls;
    _ssl = ssl;
    _ip_from = ip_from;
    _port_from = port_from;
    return true;
  }

  return false;
}

void Client::start(/*float tmo*/)
{
  if ((_w_cli = new ev::io()) != nullptr) {
    _w_cli->set(_fd_cli, ev::READ);
    _w_cli->set<Client, &Client::read_cli_cb>(this);
    _w_cli->start();
  }
  if ((_w_tls = new ev::io()) != nullptr) {
    _w_tls->set(_host.socket(), ev::READ);
    _w_tls->set<Client, &Client::read_tls_cb>(this);
    _w_tls->start();
  }
  /*if ((_w_tmo = new ev::timer()) != nullptr) {
    _w_tmo->set(tmo, 0);
    _w_tmo->set<Client, &Client::timeout_cb>(this);
    _w_tmo->start();
  }*/
}

void Client::stop()
{
  if (_w_cli != nullptr) { delete _w_cli; _w_cli = nullptr; }
  if (_w_tls != nullptr) { delete _w_tls; _w_tls = nullptr; }
  //if (_w_tmo != nullptr) { delete _w_tmo; _w_tmo = nullptr; }

  log("Closing connection [%s:%u]", _ip_from.c_str(), _port_from);

  _done = true;
}

bool Client::done()
{
  return _done;
}

void Client::cleanup()
{
  stop();

  if (_ssl != nullptr && _tls != nullptr) { _tls->close(_ssl); _ssl = nullptr; }
  _host.close(_fd_cli);
  _host.close();
}

void Client::read_cli_cb(ev::io& w, int revents)
{
  char buf[BUFSIZ];
  ssize_t len;

  if ((len = _host.recv(w.fd, buf, sizeof(buf))) > 0) _tls->write(_ssl, buf, len);
  else if (len == 0) cleanup();
  else { perror("read_cli_cb"); cleanup(); }
}

void Client::read_tls_cb(ev::io& w, int revents)
{
  char buf[BUFSIZ];
  ssize_t len;

  if ((len = _tls->read(_ssl, buf, sizeof(buf))) > 0) _host.send(_fd_cli, buf, len);
  else if (len == 0) cleanup();
  else { perror("read_tls_cb"); cleanup(); }

}

/////////////////////////////////////////////////

Server::Server() : 
  _running(false),
  _issrv(false),
  _timeout(DEF_TIMEOUT),
  _loc_addrinfo(nullptr),
  _loop(nullptr), 
  _w_soc(nullptr),
  _w_loc(nullptr),
  _w_tmo(nullptr),
  _w_sig(nullptr) {}

Server::~Server()
{
  if (! access(_pidfile.c_str(), F_OK)) remove(_pidfile.c_str());
}

bool Server::client(const string& ip_tls, int port_tls, const string& ip_local, int port_local, const string& serial, const string& nmpwd, const string& pidfl)
{
  if (! _tls.init()) {
    _tls.error();
    return false;
  }
  if (! pidfl.empty() && ! pidfile(pidfl)) {
    return false;
  }
  if (_soc.resolve(ip_tls.c_str(), port_tls, &_loc_addrinfo) != -1 && \
      _loc.socket(0x111) != -1 && _loc.bind(ip_local.c_str(), port_local) != -1 && _loc.listen() != -1) {
    if (! nmpwd.empty()) nmpwd_init(nmpwd, false);
    _serial = serial;
    _running = true;
    return true;
  } else perror("socket");
  return false;
}

bool Server::server(const string& ip_tls, int port_tls, const string& ip_web, int port_web, const string& serial, const string& nmpwd, const string& pidfl, const string& key, const string& cert, const string& page, float timeout)
{
  if (! _tls.init(key, cert)) {
    _tls.error();
    return false;
  }
  if (! pidfl.empty() && ! pidfile(pidfl)) {
    return false;
  }
  if (_soc.socket(0x111) != -1 && _soc.bind(ip_tls.c_str(), port_tls) != -1 && _soc.listen() != -1 && \
      _loc.socket(0x111) != -1 && _loc.bind(ip_web.c_str(), port_web) != -1 && _loc.listen() != -1) {
    if (! nmpwd.empty()) nmpwd_init(nmpwd, true);
    if (! web_initpage(page)) {
      _200_ctx = DEF_CTX_SUCCESS;
      _400_ctx = DEF_CTX_BADREQUEST;
      _404_ctx = DEF_CTX_NOTFOUND;
    }
    _serial = serial;
    _timeout = timeout;
    _running = true;
    return _issrv = true;
  } else perror("socket");
  return false;
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
    if ((_w_tmo = new ev::timer()) != nullptr) {
      _w_tmo->set(0, MIN(TMO_MAX, MAX(TMO_MIN, _timeout)));
      _w_tmo->set<Server, &Server::timeout_cb>(this);
      _w_tmo->start();
    }
    if ((_w_sig = new ev::sig()) != nullptr) {
      _w_sig->set(SIGINT);
      _w_sig->set<Server, &Server::signal_cb>(this);
      _w_sig->start();
    }
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
    if ((_w_tmo = new ev::timer()) != nullptr) {
      _w_tmo->set(0, MIN(TMO_MAX, MAX(TMO_MIN, _timeout)));
      _w_tmo->set<Server, &Server::timeout_cb>(this);
      _w_tmo->start();
    }
    if ((_w_sig = new ev::sig()) != nullptr) {
      _w_sig->set(SIGINT);
      _w_sig->set<Server, &Server::signal_cb>(this);
      _w_sig->start();
    }
    log("Proxy server is listening on [%s:%u]", _soc.gethostip().c_str(), _soc.getport());
    log("Web server is listening on [%s:%u]", _loc.gethostip().c_str(), _loc.getport());
    _loop->run();
  } else _running = false;
}

void Server::stop_client()
{
  for (auto& it : _lst_client) delete it;
  _lst_client.clear();
  if (_w_loc != nullptr) { delete _w_loc; _w_loc = nullptr; }
  if (_w_tmo != nullptr) { delete _w_tmo; _w_tmo = nullptr; }
  if (_w_sig != nullptr) { delete _w_sig; _w_sig = nullptr; }
  if (_loop  != nullptr) { delete _loop;  _loop  = nullptr; }
  if (_loc_addrinfo != nullptr) { _soc.resolve(nullptr, 0, &_loc_addrinfo); _loc_addrinfo = nullptr; }
  _loc.close(); // close socket
  _running = false;
}

void Server::stop_server()
{
  for (auto& it : _lst_socks5) delete it;
  _lst_socks5.clear();
  if (_w_soc != nullptr) { delete _w_soc; _w_soc = nullptr; }
  if (_w_loc != nullptr) { delete _w_loc; _w_loc = nullptr; }
  if (_w_tmo != nullptr) { delete _w_tmo; _w_tmo = nullptr; }
  if (_w_sig != nullptr) { delete _w_sig; _w_sig = nullptr; }
  if (_loop  != nullptr) { delete _loop;  _loop  = nullptr; }
  _loc.close();
  _soc.close();
  _running = false;
}

////////////////////////////////////////////

bool Server::web_hdrinfo(const Buffer& str, string& cmd, string& path)
{
  const char* p = (const char*) str.data();
  const char* r = strchr(p, ' ');

  if (*r == ' ') {
    if (r - p > 0) {
      cmd = string(p, r - p);
    } else return false;
  } else return false;

  for ( ; *r == ' '; ) r++;

  if (*r != ' ' && *r != '\0' && *r == '/') {
    const char* q = strchr(r, ' ');
    const char* m = strchr(r, '\r');
    const char* n = strchr(r, '\n');
    if (m + 1 >= n && q < m) {
      path = string(r, q - r);
      return true;
    }
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
    perror(page.c_str());
    return false;
  }
}

bool Server::nmpwd_init(const string& nmpwd, bool fl)
{
  bool okay = false;

  if (fl) {
    ifstream fin(nmpwd.c_str(), ios::in);
    string line;

    if (fin.good()) {
      while (true) {
        if (getline(fin, line)) {
          const char* s = line.c_str();
          const char* d = strchr(s, ':');
          const char* e = s + line.size();

          string name, passwd;

          if (s < d) name = string(s, d - s);
          if (d < e) passwd  = string(d + 1, e - d - 1);

          if (! name.empty() && ! passwd.empty()) {
            _nmpwd.insert(make_pair(name, passwd));
            okay = true;
          }
        }
      }
      fin.close();
    }
  } else {
    const char* s = nmpwd.c_str();
    const char* d = strchr(s, ':');
    const char* e = s + nmpwd.size();

    string name, passwd;

    if (s < d) name = string(s, d - s);
    if (d < e) passwd = string(d + 1, e - d - 1);

    if (! name.empty() && ! passwd.empty()) {
      _nmpwd.insert(make_pair(name, passwd));
      okay = true;
    }
  }

  return okay;
}

////////////////////////////////////////////

void Server::soc_new_connection(int fd, const char* ip, int port)
{
  SSL* ssl = _tls.ssl();
  SOCKS5* socks5 = new SOCKS5();

  if (ssl != nullptr && _tls.fd(ssl, fd) > 0 && _tls.accept(ssl) > 0 && soc_accept(ssl) && socks5 != nullptr && socks5->init(&_tls, ssl, fd, ip, port)) {
    socks5->start(_timeout);
    _lst_socks5.push_back(socks5);
    log("New connection from [%s:%u]", ip, port);
    return;
  } else perror("soc_new_connection");

  if (socks5 != nullptr) delete socks5;
  if (ssl != nullptr) _tls.close(ssl);
}

void Server::web_new_connection(int fd, const char* ip, int port)
{
  char buf[BUFSIZ]; size_t len;
  bool okay = false, done = false;

  while ((len = _loc.recv(fd, buf, sizeof(buf))) > 0) {
    if (! done) {
      string cmd, path;
      Buffer str; str.append(buf, len);

      if (web_hdrinfo(str, cmd, path) && cmd == "GET") {
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

  log("New connection [%s:%u] to web service", ip, port);
}

void Server::loc_new_connection(int fd, const char* ip, int port)
{
  SSL* ssl = _tls.ssl();
  Client* cli = new Client();

  if (ssl != nullptr && cli != nullptr && cli->init(_soc.gethostip(), _soc.getport(), fd, &_tls, ssl, ip, port) && loc_accept(ssl)) {
    cli->start(/*_timeout*/);
    _lst_client.push_back(cli);
    log("New connection from [%s:%u]", ip, port);
    return;
  } else perror("loc_new_connection");

  if (cli != nullptr) delete cli;
  if (ssl != nullptr) _tls.close(ssl);
}

////

bool Server::soc_accept(SSL* ssl)
{
  Buffer res;

  char buf[BUFSIZ];
  int len;

  while ((len = _tls.read(ssl, buf, sizeof(buf))) > 0) {
    res.append(buf, len);
    if ((unsigned) len < sizeof(buf)) break;
  }

  string cmd, path;

  if (web_hdrinfo(res, cmd, path)) {
    string ssr;
    if (_serial.c_str()[0] == '/')
      ssr = _serial;
    else {
      ssr = "/"; ssr += _serial;
    }
    if (cmd == "GET" && path == ssr) {
      _tls.write(ssl, (void*) _200_ctx.data(), _200_ctx.size());
      return true;
    } else {
      _tls.write(ssl, (void*) _404_ctx.data(), _404_ctx.size());
    }
  }
  
  return false;
}

bool Server::loc_accept(SSL* ssl)
{
  string req = "GET /";

  req += _serial + " HTTP/1.1\r\nHost: ";
  req += _soc.gethostip() + ":";

  char buf[64];

  snprintf(buf, sizeof(buf), "%u\r\n", _soc.getport());

  req += buf;
  req += "Content-Type: text/html\r\nConnection: keep-alive\r\nContent-Language: en\r\n\r\n";

  if (_tls.write(ssl, (void*) req.data(), req.size()) <= 0) {
    _tls.error();
    return false;
  }

  Buffer res;
  size_t len;

  while ((len = _tls.read(ssl, buf, sizeof(buf))) > 0) {
    res.append(buf, len);
    if ((unsigned) len < sizeof(buf)) break;
  }

  string str = string((const char*) res.data(), MIN(res.size(), 16));

  if (! str.compare(9, 3, "200")) return true;

  return false;
}

////////////////////////////////////////////

void Server::soc_accept_cb(ev::io& w, int revents)
{
  char ip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 2];
  int port, fd = _soc.accept(ip, port);

  if (fd != -1) {
    soc_new_connection(fd, ip, port);
  } else perror("soc_accept");
}

void Server::web_accept_cb(ev::io& w, int revents)
{
  char ip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 2];
  int port, fd = _loc.accept(ip, port);

  if (fd != -1) {
    web_new_connection(fd, ip, port);
  } else perror("web_accept");
}

void Server::loc_accept_cb(ev::io& w, int revents)
{
  char ip[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + 2];
  int port, fd = _loc.accept(ip, port);

  if (fd != -1) {
    loc_new_connection(fd, ip, port);
  } else perror("loc_accept");
}

void Server::timeout_cb(ev::timer& w, int revents)
{
  if (_issrv) {
    for (auto it = _lst_socks5.begin(); it != _lst_socks5.end(); ) {
      if ((*it)->done()) {
        auto lt = it; lt++;
        _lst_socks5.erase(it);
        it = lt;
      } else it++;
    }
  } else {
    for (auto it = _lst_client.begin(); it != _lst_client.end(); ) {
      if ((*it)->done()) {
        auto lt = it; lt++;
        _lst_client.erase(it);
        it = lt;
      } else it++;
    }
  }
}

void Server::signal_cb(ev::sig& w, int revents)
{
  stop();
  log("Exitting...");
  exit(EXIT_SUCCESS);
}

////////////////////////////////////////////

bool Server::running() const
{
  return _running;
}

/*end*/
