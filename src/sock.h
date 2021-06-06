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
#include <sys/select.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/errno.h>

#include <string>

class Buffer;

class Socks {
public:
  Socks();
  ~Socks();
  
  int socket();
  int socket(int domain, int soctyp);
  int listen(int backlog = 0);
  int connect(const struct sockaddr* addr, socklen_t addr_len, int tags = 0x01);
  int connect(const char* hostip, int port, int tags = 0x01);
  int bind(const struct sockaddr* addr, socklen_t addr_len, int tags = 0x11, bool bd = true);
  int bind(const char* hostip, int port, int tags = 0x11, bool bd = true);
  int accept(struct sockaddr* addr, socklen_t* addr_len);
  int accept(char* hostip, int& port);

  ssize_t recv(void* buf, size_t len, int flags = 0);
  ssize_t recv(int cli, void* buf, size_t len, int flags = 0);
  ssize_t recvfrom(void* buf, size_t len, struct sockaddr* addr, socklen_t* addr_len, int flags = 0);
  ssize_t recvfrom(void *buf, size_t len, char* hostip, int& port, int flags = 0);
  ssize_t send(const void* buf, size_t len, int flags = 0);
  ssize_t send(int cli, const void* buf, size_t len, int flags = 0);
  ssize_t sendto(const void* buf, size_t len, const struct sockaddr* addr, socklen_t addr_len, int flags = 0);
  ssize_t sendto(const void* buf, size_t len, const char* hostip, int port, int flags = 0);
  
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
  static int setnonblock(int soc, bool nb = true);
  int setlinger(int lg);

  static int shutdown(int& soc, int how);
  int shutdown(int how);
  static int close(int& soc);
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
