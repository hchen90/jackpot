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

using namespace std;

TLS::TLS() : _ctx(nullptr)
{
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
  if ((_ctx = SSL_CTX_new(TLS_client_method())) != nullptr) {
    SSL_CTX_set_ecdh_auto(_ctx, 1);
    return true;
  }
  return false;
}

bool TLS::init(const string& key, const string& cert)
{
  if ((_ctx = SSL_CTX_new(TLS_server_method())) != nullptr) {
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
    return true;
  }
  return false;
}

///////////////////////////////

SSL* TLS::ssl()
{
  if (_ctx != nullptr)
    return SSL_new(_ctx);
  return nullptr;
}

int TLS::fd(SSL* ssl, int fd)
{
  if (ssl != nullptr) {
    int ret = SSL_set_fd(ssl, fd);
    if (ret <= 0) error();
    else return ret;
  }
  return -1;
}

int TLS::accept(SSL* ssl)
{
  if (ssl != nullptr) {
    int ret = SSL_accept(ssl);
    if (ret <= 0) error();
    else return ret;
  }
  return -1;
}

int TLS::connect(SSL* ssl)
{
  if (ssl != nullptr) {
    int ret = SSL_connect(ssl);
    if (ret <= 0) error();
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
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }
}

void TLS::error()
{
  ERR_print_errors_fp(stderr);
}

/*end*/
