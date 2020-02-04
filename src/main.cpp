/* ***
 * @ $main.cpp
 * 
 * Copyright (C) 2019 Hsiang Chen
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

#include <iostream>
#include <string>
#include <unistd.h>

#include <execinfo.h>
#include <signal.h>

#include "config.h"
#include "conf.h"
#include "server.h"
#include "utils.h"

#define INITFAIL "Cannot initialized proxy"

using namespace std;
using namespace utils;

Server server;

void sigsegt(int sig)
{
  void* arr[32];
  size_t siz = backtrace(arr, 32);
  backtrace_symbols_fd(arr, siz, STDERR_FILENO);
  exit(EXIT_FAILURE);
}

void usage(void)
{
  cout  << PACKAGE_NAME << " version " << PACKAGE_VERSION << endl \
        << "Copyright (C) 2019-2020 Hsiang Chen" << endl \
        << "This program may be freely redistributed under the terms of the GNU General Public License" << endl \
        << endl \
        << "Usage: " << PACKAGE_NAME << " [options] [args]" << endl \
        << "Options:" << endl \
        << "  -h                show this help" << endl \
        << "  -v                show version info" << endl \
        << "  -i [tip]          set ip address for tls" << endl \
        << "  -p [tport]        set port number for tls" << endl \
        << "  -k [private_key]  set private key file" << endl \
        << "  -c [certificate]  set certificate file" << endl \
        << "  -s [serial]       set serial string" << endl \
        << "  -e [page]         set web page file" << endl \
        << "  -a [wip]          set ip address for Web/SOCKS5 server" << endl \
        << "  -w [wport]        set port number for Web/SOCKS5 server" << endl \
        << "  -t [timeout]      set timeout" << endl \
        << "  -m [pidfile]      set PID file" << endl \
        << "  -n [config]       set configuration file" << endl \
        << endl \
        << "Report bugs to <" << PACKAGE_BUGREPORT << ">." << endl;
}

int main(int argc, char* argv[])
{
  string  ip_tls, port_tls, \
          ip_web, port_web, \
          key, cert, serial, nmpwd, \
          page, cfgfile, pidfile, timeout;
  short   opt, tags = 0;

  while ((opt = getopt(argc, argv, "hvbi:p:k:c:s:e:a:w:t:m:n:")) != -1) {
    switch (opt) {
      case 'h':
        usage();
        return EXIT_SUCCESS;
      case 'v':
        cout << PACKAGE_NAME << " version " << PACKAGE_VERSION << endl;
        return EXIT_SUCCESS;
      case 'i': // tls ip
        ip_tls = optarg;
        tags |= 0x1;
        break;
      case 'p': // tls port
        port_tls = optarg;
        tags |= 0x2;
        break;
      case 'k': // key file
        key = optarg;
        tags |= 0x4;
        break;
      case 'c': // certificate file
        cert = optarg;
        tags |= 0x8;
        break;
      case 's': // serial string
        serial = optarg;
        tags |= 0x10;
        break;
      case 'e': // web page file
        page = optarg;
        tags |= 0x20;
        break;
      case 'a': // ip address of web
        ip_web = optarg;
        tags |= 0x40;
        break;
      case 'w': // port of web
        port_web = optarg;
        tags |= 0x80;
        break;
      case 't': // timeout
        timeout = optarg;
        tags |= 0x100;
        break;
      case 'm': // PID file
        pidfile = optarg;
        tags |= 0x200;
        break;
      case 'n': // configuration file
        cfgfile = optarg;
        tags |= 0x400;
        break;
      default:
        usage();
        return EXIT_FAILURE;
    }
  }

  signal(SIGPIPE, SIG_IGN);
  signal(SIGSEGV, sigsegt);

  Conf cfg;

  if (tags > 0) {
    if (tags & 0x400 && ! cfg.open(cfgfile)) {
      error(cfgfile.c_str());
    }

    if (tags & 0x1) cfg.set("tls", "ip", ip_tls);
    if (tags & 0x2) cfg.set("tls", "port", port_tls);
    if (tags & 0x4) cfg.set("main", "private_key", key);
    if (tags & 0x8) cfg.set("main", "certificate", cert);
    if (tags & 0x10) cfg.set("main", "serial", serial);
    if (tags & 0x100) cfg.set("main", "timeout", timeout);
    if (tags & 0x200) cfg.set("main", "pidfile", pidfile);

    if (cfg.get("main", "private_key", key) && cfg.get("main", "certificate", cert)) {
      if (tags & 0x20) cfg.set("web", "page", page);
      if (tags & 0x40) cfg.set("web", "ip", ip_web);
      if (tags & 0x80) cfg.set("web", "port", port_web);
      if (server.server(cfg)) server.start();
      else log(INITFAIL);
    } else {
      if (tags & 0x40) cfg.set("local", "ip", ip_web);
      if (tags & 0x80) cfg.set("local", "port", port_web);
      if (server.client(cfg)) server.start();
      else log(INITFAIL);
    }
  } else {
    usage();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/*end*/
