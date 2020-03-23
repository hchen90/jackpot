/* $ @websrv.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_WEBSRV_H_
#define	_WEBSRV_H_

#include <condition_variable>
#include <string>
#include <thread>

#include "tls.h"

#define DEF_CTX_BADREQUEST "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><head><title>Bad Request</title></head><body><h1>400 Bad Request</h1><p>Unknown request</p></body></html>"
#define DEF_CTX_NOTFOUND "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>File cannot be found</p></body></html>"
#define DEF_CTX_SUCCESS "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><head><title>Welcome</title><h1>Welcome</h1><p>This page is only for test</p></head></html>"

class Server;

class WebSrv {
public:
  WebSrv();
  ~WebSrv();

  void start(Server* srv, int fd, const std::string& ip_from, int port_from);
  void stop();
  bool done();

  time_t time();
protected:
  bool init(Server* srv, int fd, const std::string& ip_from, int port_from, SSL* ssl = nullptr);
  void transfer();

  bool _done;
  time_t _latest;
private:
  static void websrv_td(WebSrv* self, Server* srv, int fd, const std::string& ip_from, int port_from);

  Server* _server;
  int _fd_cli;
  SSL* _ssl;
  bool _running;

  std::string _ip_from;
  int _port_from;

  std::thread* _td_web;
};

#endif	/* _WEBSRV_H_ */
