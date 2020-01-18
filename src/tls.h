/* $ @tls.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_TLS_H_
#define	_TLS_H_

#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>

class TLS {
public:
  TLS();
  ~TLS();

  bool init(); // for client side
  bool init(const std::string& key, const std::string& cert); // for server side

  SSL* ssl();
  int fd(SSL* ssl, int fd);
  int accept(SSL* ssl);
  int connect(SSL* ssl);
  int read(SSL* ssl, void* buf, int num);
  int peek(SSL* ssl, void* buf, int num);
  int write(SSL* ssl, void* buf, int num);
  void close(SSL* ssl);

  void error();
private:
  SSL_CTX* _ctx;
};

#endif	/* _TLS_H_ */
