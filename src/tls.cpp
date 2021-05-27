/* ***
 * @ $tls.cpp
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
#include "tls.h"
#include "utils.h"

using namespace std;

SSLcli::SSLcli()
: port(0),
  fd(-1) {
  ip.clear();
}

TLS::TLS() : _ctx(nullptr)
{
  _sslcli.clear();
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
}

TLS::~TLS()
{
  if (_ctx != nullptr) {
    SSL_CTX_free(_ctx);
  }
  EVP_cleanup();
}

bool TLS::init()
{
  if ((_ctx = SSL_CTX_new(SSLv23_client_method())) != nullptr) {
    SSL_CTX_set_ecdh_auto(_ctx, 1);
    return true;
  }
  return false;
}

bool TLS::init(const string& key, const string& cert)
{
  if ((_ctx = SSL_CTX_new(SSLv23_server_method())) != nullptr) {
    SSL_CTX_set_ecdh_auto(_ctx, 1);
    // set key and cert
    if (SSL_CTX_use_PrivateKey_file(_ctx, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
      ERR_print_errors_fp(stderr);
      return false;
    }
    if (SSL_CTX_use_certificate_file(_ctx, cert.c_str(), SSL_FILETYPE_PEM) <= 0) {
      ERR_print_errors_fp(stderr);
      return false;
    }
    if (! SSL_CTX_check_private_key(_ctx)) {
      ERR_print_errors_fp(stderr);
      return false;
    }
    return true;
  }
  return false;
}

///////////////////////////////

SSL* TLS::ssl(const string& ip, int port)
{
  if (_ctx != nullptr) {
    SSL* s = SSL_new(_ctx);
    if (s != nullptr) {
#ifdef USE_SMARTPOINTER
      shared_ptr<SSLcli> sc = make_shared<SSLcli>();
      if (sc)
#else
      SSLcli* sc = new SSLcli();
      if (sc != nullptr)
#endif
      {
        sc->port = port;
        sc->ip = ip;
        _sslcli.insert(make_pair(s, sc));
      }
    }
    return s;
  }
  return nullptr;
}

int TLS::fd(SSL* ssl, int fd)
{
  if (ssl != nullptr) {
    int ret = SSL_set_fd(ssl, fd);
    if (ret <= 0) error(ssl);
    else {
      auto lt = _sslcli.find(ssl);
      if (lt != _sslcli.end()) {
        lt->second->fd = fd;
      }
      return ret;
    }
  }
  return -1;
}

int TLS::accept(SSL* ssl)
{
  if (ssl != nullptr) {
    int ret = SSL_accept(ssl);
    if (ret <= 0) error(ssl);
    else return ret;
  }
  return -1;
}

int TLS::connect(SSL* ssl)
{
  if (ssl != nullptr) {
    int ret = SSL_connect(ssl);
    if (ret <= 0) error(ssl);
    else return ret;
  }
  return -1;
}

int TLS::read(SSL* ssl, void* buf, int num)
{
  if (ssl != nullptr)
    return SSL_read(ssl, buf, num);
  return -1;
}

int TLS::peek(SSL* ssl, void* buf, int num)
{
  if (ssl != nullptr)
    return SSL_peek(ssl, buf, num);
  return -1;
}

int TLS::write(SSL* ssl, void* buf, int num)
{
  if (ssl != nullptr)
    return SSL_write(ssl, buf, num);
  return -1;
}

void TLS::close(SSL* ssl)
{
  if (ssl != nullptr) {
    auto sc = _sslcli.find(ssl);
    if (sc != _sslcli.end()) {
#ifndef USE_SMARTPOINTER
      delete sc->second;
#endif
      _sslcli.erase(sc);
    }
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }
}

void TLS::error(SSL* ssl)
{
  auto err = ERR_get_error();
  if (ssl != nullptr && err != 0) {
    auto sc = _sslcli.find(ssl);
    if (sc != _sslcli.end()) {
      string str = "[";

      str += sc->second->ip + ":";

      char buf[BUFSIZ]; // more bytes preserved for `ERR_error_string_n()'
      snprintf(buf, sizeof(buf), "%u", sc->second->port);
      
      str += buf;
      str += "]";

      ERR_error_string_n(err, buf, sizeof(buf));

      utils::log("%s %s", str.c_str(), buf);
      return;
    }
  }

  ERR_print_errors_fp(stderr);
}

int TLS::setnonblock(SSL* ssl, bool nb)
{
  auto lt = _sslcli.find(ssl);
  if (lt != _sslcli.end() && lt->second->fd > 0) {
    return Socks::setnonblock(lt->second->fd, nb);
  }

  return -1;
}

/*end*/
