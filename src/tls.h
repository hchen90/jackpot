/* $ @tls.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_TLS_H_
#define	_TLS_H_

#include <map>
#include <memory>
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "socks.h"
#include "config.h"

struct SSLcli {
  int port;
  std::string ip;
};

class TLS {
public:
  TLS();
  ~TLS();

  bool init(); // for client side
  bool init(const std::string& key, const std::string& cert); // for server side

  SSL* ssl(const std::string& ip, int port);
  int fd(SSL* ssl, int fd);
  int accept(SSL* ssl);
  int connect(SSL* ssl);
  int read(SSL* ssl, void* buf, int num);
  int peek(SSL* ssl, void* buf, int num);
  int write(SSL* ssl, void* buf, int num);
  void close(SSL* ssl);
  void error(SSL* ssl = nullptr);
private:
  SSL_CTX* _ctx;

  std::map<SSL*, std::shared_ptr<SSLcli>> _sslcli;
};

#endif	/* _TLS_H_ */
