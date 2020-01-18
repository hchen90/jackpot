/* $ @socks.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_SOCKS_H_
#define	_SOCKS_H_

#include <openssl/ssl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#include <string>
#include <list>

class Buffer;

class Socks {
public:
  Socks();
  ~Socks();
  
  int socket();
  int socket(int type); /*  type & 0x01: IP4, 
                            type & 0x02: IP6, 
                            type & 0x04: UNIX,
                            type & 0x10: STREAM, 
                            type & 0x20: DATAGRAM
                            type & 0x100: REUSEADDR
                            type & 0x200: REUSEPORT */
  int socket(int domain, int soctyp);
  int listen(int backlog = 32);
  int connect(const struct sockaddr* addr, socklen_t addr_len);
  int connect(const char* hostip, int port);
  int connect(const char* file);
  int bind(const struct sockaddr* addr, socklen_t addr_len);
  int bind(const char* hostip, int port);
  int bind(const char* file);
  int accept(struct sockaddr* addr, socklen_t* addr_len);
  int accept(char* hostip, int& port);

  ssize_t recv(Buffer& str, int flags = 0); // only for non-blocking socket
  ssize_t recv(int cli, Buffer& str, int flags = 0); // only for non-blocking socket
  ssize_t recv(void* buf, size_t len, int flags = 0);
  ssize_t recv(int cli, void* buf, size_t len, int flags = 0);
  ssize_t recvfrom(void* buf, size_t len, struct sockaddr* addr, socklen_t* addr_len, int flags = 0);
  ssize_t recvfrom(void *buf, size_t len, char* hostip, int& port, int flags = 0);
  ssize_t send(const void* buf, size_t len, int flags = 0);
  ssize_t send(int cli, const void* buf, size_t len, int flags = 0);
  ssize_t sendto(const void* buf, size_t len, const struct sockaddr* addr, socklen_t addr_len, int flags = 0);
  ssize_t sendto(const void* buf, size_t len, const char* hostip, int port, int flags = 0);
  
  ssize_t read(void* buf, size_t len);
  ssize_t write(const void* buf, size_t count);

  int getsockopt(int level, int optname, void* optval, socklen_t* optlen);
  int getsockopt(int soc, int level, int optname, void* optval, socklen_t* optlen);
  int setsockopt(int level, int optname, const void* optval, socklen_t optlen);
  int setsockopt(int soc, int level, int optname, void* optval, socklen_t optlen);
  int getsockname(struct sockaddr* addr, socklen_t* addr_len);
  int getsockname(int soc, struct sockaddr* addr, socklen_t* addr_len);
  int getpeername(struct sockaddr* addr, socklen_t* addr_len);
  int getpeername(int soc, struct sockaddr* addr, socklen_t* addr_len);

  const std::string& gethostip() const;
  int getport() const;

  int setnonblock(bool nb = true);
  int setnonblock(int soc, bool nb = true);
  int setlinger(int lg);

  int shutdown(int how = SHUT_RDWR);
  int close(int& soc);
  int close();

  int resolve(const char* hostip, int port, struct addrinfo** addr); // call this function with hostip = nullptr to free resource
  int resolve(const struct sockaddr* addr, char* hostip, int& port); // set addr_len to sizeof(addr) before call accept() if you want to resolve its ip and port
private:
  int socket_dm;
  int socket_ty;
  int socket_fd;
  std::string socket_hostip;
  int socket_port;
};

#endif	/* _SOCKS_H_ */
