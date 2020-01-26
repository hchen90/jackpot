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
#include <signal.h>

#include "config.h"
#include "conf.h"
#include "server.h"
#include "utils.h"

#define INITFAIL "Cannot initialized proxy"

using namespace std;

Server server;

void usage(void)
{
  cout  << PACKAGE_NAME << " version " << PACKAGE_VERSION << endl \
        << "Copyright (C) 2019 Hsiang Chen" << endl \
        << "This program may be freely redistributed under the terms of the GNU General Public License" << endl \
        << endl \
        << "Usage: " << PACKAGE_NAME << " [options] [args]" << endl \
        << "Options:" << endl \
        << "  -h                show this help" << endl \
        << "  -v                show version info" << endl \
        << "  -4                force IP4" << endl \
        << "  -6                force IP6" << endl \
        << "  -b                use multiple threads" << endl \
        << "  -i [tip]          set ip address for tls" << endl \
        << "  -p [tport]        set port number for tls" << endl \
        << "  -k [private_key]  set private key file" << endl \
        << "  -c [certificate]  set certificate file" << endl \
        << "  -s [serial]       set serial string" << endl \
        << "  -e [page]         set web page file" << endl \
        << "  -a [wip]          set ip address for web service or SOCKS5 server" << endl \
        << "  -w [wport]        set port number for web service or SOCKS5 server" << endl \
        << "  -t [timeout]      set timeout" << endl \
        << "  -x [nmpwd]        set username and password for SOCKS5 server" << endl \
        << "  -m [config]       set configuration file" << endl \
        << "  -n [pidfile]      set PID file" << endl \
        << endl \
        << "Report bugs to <" << PACKAGE_BUGREPORT << ">." << endl;
}

int main(int argc, char* argv[])
{
  int opt, port_tls = 443, port_web = 0;
  string ip_tls = "0.0.0.0", ip_web = "0.0.0.0", key, cert, serial, nmpwd, page, cfgfile, pidfile;
  short tags = 0; float timeout = 20.0;

  while ((opt = getopt(argc, argv, "hvi:p:k:c:s:e:a:w:t:x:m:n:")) != -1) {
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
        port_tls = atoi(optarg);
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
        port_web = atoi(optarg);
        tags |= 0x80;
        break;
      case 't': // timeout
        timeout = atof(optarg);
        tags |= 0x100;
        break;
      case 'x': // username and password
        nmpwd = optarg;
        tags |= 0x200;
        break;
      case 'm': // configuration
        cfgfile = optarg;
        tags |= 0x400;
        break;
      case 'n': // PID file
        pidfile = optarg;
        tags |= 0x800;
        break;
      default:
        usage();
        return EXIT_FAILURE;
    }
  }

  signal(SIGPIPE, SIG_IGN);

  if (tags & 0x400) {
    Conf cfg;

    if (cfg.open(cfgfile)) {
      string str;

      cfg.get("tls", "ip", ip_tls);
      cfg.get("main", "serial", serial);
      cfg.get("main", "pidfile", pidfile);

      if (cfg.get("main", "timeout", str))
        timeout = atof(str.c_str());
      if (cfg.get("tls", "port", str))
        port_tls = atoi(str.c_str());
      if (cfg.get("main", "private_key", key) && cfg.get("main", "certificate", cert)) {
        cfg.get("web", "ip", ip_web);
        cfg.get("web", "page", page);
        cfg.get("user", "file", nmpwd);
        
        if (cfg.get("web", "port", str))
          port_web = atoi(str.c_str());
        if (port_web <= 0)
          port_web = 80;
        if (server.server(ip_tls, port_tls, ip_web, port_web, serial, nmpwd, pidfile, key, cert, page, timeout))
          server.start();
        else
          log(INITFAIL);
      } else {
        cfg.get("local", "ip", ip_web);
        
        string nam, pwd;

        if (cfg.get("user", "name", nam) && cfg.get("user", "password", pwd)) {
          nmpwd = nam + ":";
          nmpwd += pwd;
        }
        if (cfg.get("local", "port", str))
          port_web = atoi(str.c_str());
        if (port_web <= 0)
          port_web = 1080;
        if (server.client(ip_tls, port_tls, ip_web, port_web, serial, nmpwd, pidfile))
          server.start();
        else
          log(INITFAIL);
      }
    }
  } else if ((tags & 0x1c) == 0x1c) {
    // server
    if (port_web <= 0)
      port_web = 80;
    if (server.server(ip_tls, port_tls, ip_web, port_web, serial, nmpwd, pidfile, key, cert, page, timeout))
      server.start();
    else
      log(INITFAIL);
  } else if ((tags & 0x11) == 0x11) {
    // client
    if (port_web <= 0)
      port_web = 1080;
    if (server.client(ip_tls, port_tls, ip_web, port_web, serial, nmpwd, pidfile))
      server.start();
    else
      log(INITFAIL);
  } else {
    usage();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/*end*/
