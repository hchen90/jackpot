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

#include <ev++.h>

#include "conf.h"
#include "socks.h"
#include "socks5.h"

#define TMO_MAX 50
#define TMO_MIN 5

class Client {
public:
  Client();
  ~Client();

  bool init(const std::string& hostip, int port, int fd, TLS* tls, SSL* ssl, const std::string& ip_from, int port_from);
  void start(std::condition_variable* cv, time_t tmo);
  void stop();
  bool done();
private:
  void cleanup();
  bool read_cli();
  bool read_tls();

  static void transfer_td(Client* self);

  Socks _host;

  TLS* _tls;
  SSL* _ssl;

  int _fd_cli;

  bool _done, _multi, _running;
  time_t _timeout;

  std::string _ip_from;
  int _port_from;

  std::thread* _td_trf;
  std::condition_variable* _cv_cleanup;
};

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

  bool web_hdrinfo(const void* ptr, size_t len, std::string& cmd, std::string& path);
  bool web_initpage(const std::string& page);
  void socks5_initnmpwd(Conf& cfg);

  void start_client();
  void start_server();
  void stop_client();
  void stop_server();

  void soc_accept_cb(ev::io& w, int revents);
  void web_accept_cb(ev::io& w, int revents);
  void loc_accept_cb(ev::io& w, int revents);
  void signal_cb(ev::sig& w, int revents);
  void segment_cb(ev::sig& w, int revents);

  void soc_new_connection(int fd, const char* ip, int port);
  void web_new_connection(int fd, const char* ip, int port);
  void loc_new_connection(int fd, const char* ip, int port);

  bool soc_accept(SSL* ssl);
  bool loc_accept(SSL* ssl);

  static void cleanup_td(Server* self);

  ///////////////////////////////////////////////
  
  bool _running, _issrv;
  float _timeout;

  TLS _tls;

  Socks _soc; // default: [server] port 443
              // default: [client] port 443 (only for storage of ip & port)

  Socks _loc; // default: [server] port 80
              // default: [client] port 1080

  std::string _200_ctx, _400_ctx, _404_ctx, _serial, _pidfile;
  std::map<std::string, std::string> _nmpwd;

  std::list<SOCKS5*> _lst_socks5;
  std::list<Client*> _lst_client;

  struct addrinfo* _loc_addrinfo;

  ev::default_loop* _loop;
  ev::io* _w_soc;
  ev::io* _w_loc;
  ev::sig* _w_sig;
  ev::sig* _w_seg;

  std::condition_variable _cv_cleanup;
  std::mutex _mutex_cleanup;
  std::thread* _td_cleanup;
};

#endif	/* _SERVER_H_ */
