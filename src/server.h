/* $ @server.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_SERVER_H_
#define	_SERVER_H_

#include <string>
#include <map>

#include <ev++.h>

#include "socks.h"
#include "socks5.h"

#define TMO_MAX 50
#define TMO_MIN 5

class Client {
public:
  Client();
  ~Client();

  bool init(const std::string& hostip, int port, int fd, TLS* tls, SSL* ssl, const std::string& ip_from, int port_from);
  void start();
  void stop();
  bool done();
private:
  void read_cli_cb(ev::io& w, int revents);
  void read_tls_cb(ev::io& w, int revents);
  void timeout_cb(ev::timer& w, int revents);

  Socks _host;

  TLS* _tls;
  SSL* _ssl;

  int _fd_cli;

  ev::io* _w_cli;
  ev::io* _w_tls;

  bool _done;

  std::string _ip_from;
  int _port_from;
};

class Server {
public:
  Server();
  ~Server();

  bool client(const std::string& ip_tls, int port_tls, const std::string& ip_local, int port_local, const std::string& serial, const std::string& nmpwd); // local SOCKS5 server
  bool server(const std::string& ip_tls, int port_tls, const std::string& ip_web, int port_web, const std::string& serial, const std::string& nmpwd, const std::string& key, const std::string& cert, const std::string& page, float timeout); // remote SOCKS5-over-TLS server

  void start();
  void stop();

  bool running() const;
private:
  bool web_hdrinfo(const Buffer& str, std::string& cmd, std::string& path);
  bool web_initpage(const std::string& page);
  bool nmpwd_init(const std::string& nmpwd, bool fl);

  void start_client();
  void start_server();
  void stop_client();
  void stop_server();

  void soc_accept_cb(ev::io& w, int revents);
  void web_accept_cb(ev::io& w, int revents);
  void loc_accept_cb(ev::io& w, int revents);
  void timeout_cb(ev::timer& w, int revents);
  void signal_cb(ev::sig& w, int revents);

  void soc_new_connection(int fd, const char* ip, int port);
  void web_new_connection(int fd, const char* ip, int port);
  void loc_new_connection(int fd, const char* ip, int port);

  bool soc_accept(SSL* ssl);
  bool loc_accept(SSL* ssl);

  ///////////////////////////////////////////////
  
  bool _running, _issrv;
  float _timeout;

  TLS _tls;

  Socks _soc; // default: [server] port 443
              // default: [client] port 443 (only for storage of ip & port)

  Socks _loc; // default: [server] port 80
              // default: [client] port 1080

  std::string _200_ctx, _400_ctx, _404_ctx, _serial;
  std::map<std::string, std::string> _nmpwd;
  std::map<int, SOCKS5*> _mp_socks5;
  std::map<int, Client*> _mp_client;

  struct addrinfo* _loc_addrinfo;

  ev::default_loop* _loop;
  ev::io* _w_soc;
  ev::io* _w_loc;
  ev::timer* _w_tmo;
  ev::sig* _w_sig;
};

#endif	/* _SERVER_H_ */
