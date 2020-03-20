/* $ @server.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_SERVER_H_
#define	_SERVER_H_

#include <condition_variable>
#include <string>
#include <thread>
#include <list>
#include <map>
#include <memory>
#include <mutex>

#include <ev++.h>

#include "conf.h"
#include "socks.h"
#include "socks5.h"
#include "client.h"
#include "websv.h"
#include "ctxwrapper.h"

class Server {
public:
  Server();
  ~Server();

  bool client(Conf& cfg); // local SOCKS5 server
  bool server(Conf& cfg); // remote SOCKS5-over-TLS server
  void start();
  void stop();
private:
  bool pidfile(const std::string& pidfl);

  bool web_hdrinfo(const void* ptr, size_t len, std::string& cmd, std::string& path, std::string& ver);
  void socks5_initnmpwd(Conf& cfg);

  void start_client();
  void start_server();
  void stop_client();
  void stop_server();

  void soc_accept_cb(ev::io& w, int revents);
  void web_accept_cb(ev::io& w, int revents);
  void loc_accept_cb(ev::io& w, int revents);
  void signal_cb(ev::sig& w, int revents);
  void timeout_cb(ev::timer& w, int revents);

  void soc_new_connection(int fd, const char* ip, int port);
  void web_new_connection(int fd, const char* ip, int port);
  void loc_new_connection(int fd, const char* ip, int port);

  bool loc_accept(SSL* ssl);
  bool soc_accept(SSL* ssl);

  static void cleanup_td(Server* self);

  ///////////////////////////////////////////////
  
  bool _running, _issrv, _norootfs;
  time_t _ctimeout, _stimeout;

  CtxWrapper _ctxwrapper;

  TLS _tls;

  Socks _soc; // default: [server] port 443
              // default: [client] port 443 (only for storage of ip & port)

  Socks _loc; // default: [server] port 80
              // default: [client] port 1080

  std::string _serial, _pidfile;
  std::map<std::string, std::string> _nmpwd;

  std::list<std::shared_ptr<SOCKS5>> _lst_socks5;
  std::list<std::shared_ptr<Client>> _lst_client;
  std::list<std::shared_ptr<WebSv>> _lst_websv;

  struct addrinfo* _loc_addrinfo;

  ev::default_loop* _loop;
  ev::io* _w_soc;
  ev::io* _w_loc;
  ev::sig* _w_sig;

  std::condition_variable _cv_cleanup;
  std::mutex _mutex_cleanup, _mutex_inprogress;
  std::thread* _td_cleanup;

  friend Client;
  friend SOCKS5;
  friend WebSv;
};

#endif	/* _SERVER_H_ */
