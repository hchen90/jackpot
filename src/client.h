/* $ @client.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_CLIENT_H_
#define	_CLIENT_H_

#include <condition_variable>
#include <string>
#include <thread>

#include "conf.h"
#include "tls.h"

class Server;

class Client {
public:
  Client();
  ~Client();

  void start(Server* srv, int fd, const std::string& ip_from, int port_from);
  void stop();
  bool done();

  time_t time();
private:
  bool read_cli();
  bool read_tls();

  bool init(Server* srv, int fd, const std::string& ip_from, int port_from);
  void transfer();

  static void client_td(Client* self, Server* srv, int fd, const std::string& ip_from, int port_from);

  Socks _host;

  TLS* _tls;
  SSL* _ssl;

  int _fd_cli;

  bool _done, _running;
  time_t _timeout, _latest;

  std::string _ip_from;
  int _port_from;

  std::thread* _td_trf;
  std::condition_variable* _cv_cleanup;
};

#endif	/* _CLIENT_H_ */
