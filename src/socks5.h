/* $ @socks5.h
 * Copyright (C) 2019 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_SOCKS5_H_
#define	_SOCKS5_H_

#include <condition_variable>
#include <string>
#include <thread>
#include <map>

#include "sock.h"
#include "tls.h"
#include "websrv.h"

#define SOCKS5_VER '\x05'
#define SOCKS5_AUTHVER '\x01'

#define SOCKS5_METHOD_NOAUTH '\x00'
#define SOCKS5_METHOD_GSSAPI '\x01'
#define SOCKS5_METHOD_USRPASS '\x02'
#define SOCKS5_METHOD_UNACCEPT '\xff'

#define SOCKS5_CMD_CONNECT '\x01'
#define SOCKS5_CMD_BIND '\x02'
#define SOCKS5_CMD_UDP '\x03'

#define SOCKS5_ATYP_IPV4 '\x01'
#define SOCKS5_ATYP_DOMAINNAME '\x03'
#define SOCKS5_ATYP_IPV6 '\x04'

#define SOCKS5_REP_ERROR '\xff'
#define SOCKS5_REP_SUCCESS '\x00'
#define SOCKS5_REP_SRVFAILURE '\x01'
#define SOCKS5_REP_NOTALLOWED '\x02'
#define SOCKS5_REP_NETUNREACH '\x03'
#define SOCKS5_REP_HOSTUNREACH '\x04'
#define SOCKS5_REP_REFUSED '\x05'
#define SOCKS5_REP_TTLEXPI '\x06'
#define SOCKS5_REP_CMDUNSUPPORTED '\x07'
#define SOCKS5_REP_ADDRUNSUPPORTED '\x08'

#define SOCKS5_SUCCESS SOCKS5_REP_SUCCESS
#define SOCKS5_ERROR SOCKS5_REP_ERROR

#define STAGE_INIT 0
#define STAGE_AUTH 1
#define STAGE_REQU 2
#define STAGE_CONN 3
#define STAGE_BIND 4
#define STAGE_UDPP 5
#define STAGE_FINI 6

#define STATUS_IPV4_LENGTH 10
#define STATUS_IPV6_LENGTH 22

class Server;

class SOCKS5 : public WebSrv {
public:
  SOCKS5();
  ~SOCKS5();

  void start(Server* srv, int fd, const std::string& ip_from, int port_from);
  void stop();
private:
  void timeout();
  bool read_tls();
  bool read_tgt();

  short stage_init(void* ptr, size_t len);
  short stage_auth(void* ptr, size_t len);
  short stage_requ(void* ptr, size_t len);

  short stage_conn();
  short stage_bind();
  short stage_udpp();

  bool init(Server* srv, int fd, const std::string& ip_from, int port_from);
  void transfer();

  static void socks5_td(SOCKS5* self, Server* srv, int fd, const std::string& ip_from, int port_from);

  int _fd_tls, _port_from;
  bool _running, _iswebsrv;
  short _stage;

  std::string _ip_from;
  Socks _target;
  SSL* _ssl;
  std::thread* _td_socks5;
  Server* _server;
};

#endif	/* _SOCKS5_H_ */
