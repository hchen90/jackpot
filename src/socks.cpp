/* ***
 * @ $socks.cpp
 * 
 * Copyright (C) 2018 Hsiang Chen
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netdb.h>
#include <fcntl.h>
#include <cstdlib>

#include "config.h"
#include "socks.h"

using namespace std;

Socks::Socks() : socket_dm(0), socket_ty(0), socket_fd(-1), socket_port(0) {}

Socks::~Socks()
{
  close();
}

int Socks::socket()
{
  return socket_fd;
}

int Socks::socket(int domain, int soctyp)
{
  return socket_fd = ::socket(socket_dm = domain, socket_ty = soctyp, 0);
}

int Socks::listen(int backlog)
{
  if (socket_ty == SOCK_STREAM) {
    return ::listen(socket_fd, backlog);
  } else {
    return 0; // always successful
  }
}

int Socks::connect(const struct sockaddr* addr, socklen_t addr_len, int tags)
{
  return bind(addr, addr_len, tags, false);
}

int Socks::connect(const char* hostip, int port, int tags)
{
  return bind(hostip, port, tags, false);
}

/* tags:
 *  0x01 - SOCK_STREAM
 *  0x02 - SOCK_DGRAM
 *  0x10 - SO_REUSEADDR
 *  0x20 - SO_REUSEPORT
 * */
int Socks::bind(const struct sockaddr* addr, socklen_t addr_len, int tags, bool bd)
{
  if (socket_fd == -1 && socket(addr->sa_family, tags & 0x02 ? SOCK_DGRAM : SOCK_STREAM) > 0) {
    int opt = 1;

    if (tags & 0x10) setsockopt(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (tags & 0x20) setsockopt(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  }

  if (bd) return ::bind(socket_fd, addr, addr_len);

  int fl = ::fcntl(socket_fd, F_GETFL, 0);
  if (fl != -1 && fl & O_NONBLOCK) {
    return ::connect(socket_fd, addr, addr_len);
  }

  bool okay = false;
  setnonblock(true); // set socket to non-blocking
  int ret = ::connect(socket_fd, addr, addr_len);

  if (ret == -1) {
    if (errno == EINPROGRESS) {
      fd_set fds; FD_ZERO(&fds); FD_SET(socket_fd, &fds);
      struct timeval tmv = { .tv_sec = 2, .tv_usec = 0 };
      for (int i = 0; i < 4; i++) {
        if ((ret = select(socket_fd + 1, nullptr, &fds, nullptr, &tmv)) < 0 && errno != EINTR) {
          break;
        } else if (ret > 0) {
          int val; socklen_t len = sizeof(val);
          getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &val, &len);
          if (val == 0) okay = true;
          break;
        }
      }
    }
  } else okay = true;

  setnonblock(false); // set it back to blocking mode
  return okay ? 0 : -1;
}

int Socks::bind(const char* hostip, int port, int tags, bool bd)
{
  if (hostip != nullptr) {
    int rev = -1;
    struct addrinfo* addr = NULL;
    socket_hostip = hostip;
    socket_port = port;
    if (! resolve(hostip, port, &addr)) {
      for (struct addrinfo* ai = addr; ai != nullptr; ai = ai->ai_next) {
        if (bd) {
          if ((rev = bind(addr->ai_addr, addr->ai_addrlen)) != -1) break;
        } else {
          if ((rev = connect(addr->ai_addr, addr->ai_addrlen)) != -1) break;
        }
      }
      resolve(NULL, 0, &addr);
    }
    return rev;
  } else return -1;
}

int Socks::accept(struct sockaddr* addr, socklen_t* addr_len)
{
  return ::accept(socket_fd, addr, addr_len);
}

int Socks::accept(char* hostip, int& port)
{
  int soc;
  union {
    struct sockaddr addr;
    struct sockaddr_in addr_in;
    struct sockaddr_in6 addr_in6;
  } addr_dat;
  struct sockaddr* addr;
  socklen_t addr_len;
  switch (socket_dm) {
    case AF_INET:
      addr = (struct sockaddr*) &addr_dat.addr_in;
      addr_len = sizeof(addr_dat.addr_in);
      break;
    case AF_INET6:
      addr = (struct sockaddr*) &addr_dat.addr_in6;
      addr_len = sizeof(addr_dat.addr_in6);
      break;
    default:
      addr = (struct sockaddr*) &addr_dat.addr;
      addr_len = sizeof(addr_dat.addr);
      break;
  }
  if ((soc = accept(addr, &addr_len)) > 0) {
    if (resolve(addr, hostip, port) != 0) {
      close(soc);
    }
  }
  return soc;
}

ssize_t Socks::recv(void* buf, size_t len, int flags)
{
  return ::recv(socket_fd, buf, len, flags);
}

ssize_t Socks::recv(int cli, void* buf, size_t len, int flags)
{
  return ::recv(cli, buf, len, flags);
}

ssize_t Socks::recvfrom(void* buf, size_t len, struct sockaddr* addr, socklen_t* addr_len, int flags)
{
  return ::recvfrom(socket_fd, buf, len, flags, addr, addr_len);
}

ssize_t Socks::recvfrom(void* buf, size_t len, char* hostip, int& port, int flags)
{
  ssize_t rev = -1;
  struct sockaddr addr;
  socklen_t addr_len = sizeof(addr);

  if ((rev = recvfrom(buf, len, &addr, &addr_len, flags)) > 0) {
    resolve(&addr, hostip, port);
  }

  return rev;
}

ssize_t Socks::send(const void* buf, size_t len, int flags)
{
  return ::send(socket_fd, buf, len, flags);
}

ssize_t Socks::send(int cli, const void* buf, size_t len, int flags)
{
  return ::send(cli, buf, len, flags);
}

ssize_t Socks::sendto(const void* buf, size_t len, const struct sockaddr* addr, socklen_t addr_len, int flags)
{
  return ::sendto(socket_fd, buf, len, flags, addr, addr_len);
}

ssize_t Socks::sendto(const void* buf, size_t len, const char* hostip, int port, int flags)
{
  ssize_t rev = -1;
  struct addrinfo* addr = NULL;

  if (! resolve(hostip, port, &addr)) {
    rev = sendto(buf, len, addr->ai_addr, addr->ai_addrlen, flags);
    resolve(NULL, 0, &addr); // free addrinfo
  }

  return rev;
}

int Socks::getsockopt(int soc, int level, int optname, void* optval, socklen_t* optlen)
{
  return ::getsockopt(soc, level, optname, optval, optlen);
}

int Socks::getsockopt(int level, int optname, void* optval, socklen_t* optlen)
{
  return ::getsockopt(socket_fd, level, optname, optval, optlen);
}

int Socks::setsockopt(int soc, int level, int optname, void* optval, socklen_t optlen)
{
  return ::setsockopt(soc, level, optname, optval, optlen);
}

int Socks::setsockopt(int level, int optname, const void* optval, socklen_t optlen)
{
  return ::setsockopt(socket_fd, level, optname, optval, optlen);
}

int Socks::getsockname(int soc, struct sockaddr* addr, socklen_t* addr_len)
{
  return ::getsockname(soc, addr, addr_len);
}

int Socks::getsockname(struct sockaddr* addr, socklen_t* addr_len)
{
  return ::getsockname(socket_fd, addr, addr_len);
}

int Socks::getpeername(int soc, struct sockaddr* addr, socklen_t* addr_len)
{
  return ::getpeername(soc, addr, addr_len);
}

int Socks::getpeername(struct sockaddr* addr, socklen_t* addr_len)
{
  return ::getpeername(socket_fd, addr, addr_len);
}

const string& Socks::gethostip() const
{
  return socket_hostip;
}

int Socks::getport() const
{
  return socket_port;
}

int Socks::setnonblock(bool nb)
{
  return Socks::setnonblock(socket_fd, nb);
}

int Socks::setnonblock(int soc, bool nb)
{
  int ret = -1;

  if (soc > 0) {
    int flags = ::fcntl(soc, F_GETFL, 0);
    if (flags != -1) {
      if (nb)
        flags |= O_NONBLOCK;
      else
        flags &= ~O_NONBLOCK;
      ret = ::fcntl(soc, F_SETFL, flags);
    }
  }

  return ret;
}

int Socks::setlinger(int lg)
{
  struct linger lgr = {0};

  if (lg > 0) {
    lgr.l_onoff = 1;
    lgr.l_linger = lg;
  }

  return setsockopt(socket_fd, SOL_SOCKET, SO_LINGER, &lgr, sizeof(lgr));
}

int Socks::shutdown(int how)
{
  return ::shutdown(socket_fd, how);
}

int Socks::close()
{
  return close(socket_fd);
}

int Socks::close(int& soc)
{
  if (soc != -1) {
    int n = ::close(soc);
    soc = -1;
    return n;
  } else return -1;
}

int Socks::resolve(const char* hostip, int port, struct addrinfo** addr)
{
  if (hostip != NULL) {
    struct addrinfo hints = {
      .ai_flags = AI_PASSIVE,
      .ai_family = AF_UNSPEC,
      .ai_socktype = SOCK_STREAM,
      .ai_protocol = 0
    };

    if (socket_hostip.empty()) socket_hostip = hostip;
    if (socket_port <= 0) socket_port = port;

    char sport[32];

    snprintf(sport, sizeof(sport), "%u", (unsigned int) port);

    return ::getaddrinfo(hostip, sport, &hints, addr);
  } else {
    if (addr != NULL && *addr != NULL) {
      freeaddrinfo(*addr);
      return 0;
    } else return -1;
  }
}

int Socks::resolve(const struct sockaddr* addr, char* hostip, int& port)
{
  if (addr != NULL) {
    const void* src = NULL;

    if (addr->sa_family == AF_INET) {
      struct sockaddr_in* in = (struct sockaddr_in*) addr;
      src = &in->sin_addr.s_addr;
      port = ntohs(in->sin_port);
    } else if (addr->sa_family == AF_INET6) {
      struct sockaddr_in6* in6 = (struct sockaddr_in6*) addr;
      src = &in6->sin6_addr;
      port = ntohs(in6->sin6_port);
    }
    if (src != NULL && inet_ntop(addr->sa_family, src, hostip, MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)) != NULL) {
      return 0;
    }
  }

  return -1;
}

/*end*/
