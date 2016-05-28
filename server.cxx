#include <cassert>
#include <cstring>
#include <iostream>
#include <cstdlib>   

#ifndef WIN32
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#ifdef HAVE_LIBEV
#include <ev.h>
#endif

#include "udp.h"
#include "stun.h"

using namespace std;

void selectServerLoop(StunServerInfo &info, bool verboseStatistics, bool verbose);
void epollServerLoop(StunServerInfo &info, bool verboseStatistics, bool verbose);

#ifdef HAVE_LIBEV
void evServerLoop(StunServerInfo &info, bool verboseStatistics, bool verbose);
#endif

void usage() {
    cerr << "Usage: " << endl
         << " ./stun_server [-v] [-h] [-e] [s] [-h IP_Address] [-a IP_Address] [-u IP_Address/IP_Address] [-p port] "
                 "[-o port] [-d port] [-b] [-m mediaport] [-t number]" << endl
         << " " << endl
         << " If the IP addresses of your NIC are 10.0.1.150 and 10.0.1.151, run this program with" << endl
         << "    ./stun_server -v  -h 10.0.1.150 -a 10.0.1.151" << endl
         << " STUN servers need two IP addresses and two ports, these can be specified with:" << endl
         << "  -e whether receive msg with epoll mechanism" << endl
         << "  -s whether display statistics logs" << endl
         << "  -h sets the primary IP" << endl
         << "  -a sets the secondary IP" << endl
         << "  -u sets the unbind secondary IP" << endl
         << "  -p sets the primary port and defaults to 3478" << endl
         << "  -o sets the secondary port and defaults to 3479" << endl
         << "  -d sets the unbind secondary communication port and defaults to 3481" << endl
         << "  -b makes the program run in the background" << endl
         << "  -m sets up a STERN server starting at port m" << endl
         << "  -t process number" << endl
         << "  -v runs in verbose mode" << endl
         // in makefile too
         << endl;
}

int main(int argc, char* argv[]) {
    assert( sizeof(UInt8 ) == 1);
    assert( sizeof(UInt16) == 2);
    assert( sizeof(UInt32) == 4);

    initNetwork();

    clog << "STUN server version " << STUN_VERSION << endl;

    StunAddress4 myAddr;
    StunAddress4 altAddr;
    StunAddress4 unbindAltAddr;
    StunAddress4 unbindAltComAddr;
    bool verbose = false;
    bool background = false;
    bool useEpoll = false;
    bool verboseStatistics = false;

    myAddr.addr = 0;
    altAddr.addr = 0;
    myAddr.port = STUN_PORT;
    altAddr.port = STUN_PORT + 1;

    unbindAltAddr.addr = 0;
    unbindAltAddr.port = STUN_PORT + 1;
    unbindAltComAddr.addr = 0;
    unbindAltComAddr.port = 3481;

    int myPort = 0;
    int altPort = 0;
    int myMediaPort = 0;

    UInt32 interfaces[10];
    int numInterfaces = stunFindLocalInterfaces(interfaces, 10);

    if (numInterfaces == 1) {
        myAddr.addr = interfaces[0];
        myAddr.port = STUN_PORT;
    }

    int processNumber = 0;
    for (int arg = 1; arg < argc; arg++) {
        if (!strcmp(argv[arg], "-v")) {
            verbose = true;
        } else if (!strcmp(argv[arg], "-b")) {
            background = true;
        } else if (!strcmp(argv[arg], "-e")) {
            useEpoll = true;
        } else if (!strcmp(argv[arg], "-s")) {
            verboseStatistics = true;
        } else if (!strcmp(argv[arg], "-h")) {
            arg++;
            if (argc <= arg) {
                usage();
                exit(-1);
            }
            stunParseServerName(argv[arg], myAddr);
        } else if (!strcmp(argv[arg], "-a")) {
            arg++;
            if (argc <= arg) {
                usage();
                exit(-1);
            }
            stunParseServerName(argv[arg], altAddr);
        } else if (!strcmp(argv[arg], "-u")) {
            arg++;
            if (argc <= arg) {
                usage();
                exit(-1);
            }
            char *split = strchr(argv[arg], '/');
            if (split != NULL) {
                *split = 0;
                stunParseServerName(argv[arg], unbindAltAddr);
                stunParseServerName(split + 1, unbindAltComAddr);
                *split = '/';
            }
        } else if (!strcmp(argv[arg], "-p")) {
            arg++;
            if (argc <= arg) {
                usage();
                exit(-1);
            }
            myPort = UInt16(strtol(argv[arg], NULL, 10));
        } else if (!strcmp(argv[arg], "-o")) {
            arg++;
            if (argc <= arg) {
                usage();
                exit(-1);
            }
            altPort = UInt16(strtol(argv[arg], NULL, 10));
        } else if (!strcmp(argv[arg], "-d")) {
            arg++;
            if (argc <= arg) {
                usage();
                exit(-1);
            }
            unbindAltComAddr.port = UInt16(strtol(argv[arg], NULL, 10));
        } else if (!strcmp(argv[arg], "-m")) {
            arg++;
            if (argc <= arg) {
                usage();
                exit(-1);
            }
            myMediaPort = UInt16(strtol(argv[arg], NULL, 10));
        } else if (!strcmp(argv[arg], "-t")) {
            arg++;
            if (argc <= arg) {
                usage();
                exit(-1);
            }
            processNumber = strtol(argv[arg], NULL, 10);
        } else {
            usage();
            exit(-1);
        }
    }

    if (myPort != 0) {
        myAddr.port = myPort;
    }
    if (altPort != 0) {
        altAddr.port = altPort;
    }

    if ((myAddr.addr == 0) || (altAddr.addr == 0 && unbindAltAddr.addr == 0)) {
        clog << "If your machine does not have exactly two ethernet interfaces, "
             << "you must specify the server and alt server" << endl;
        usage();
        exit(-1);
    }

    if ((myAddr.port == 0) || (altAddr.port == 0 )) {
        cerr << "Bad command line" << endl;
        exit(1);
    }

    if (myAddr.port == altAddr.port) {
        altAddr.port = myAddr.port + 1;
    }

    if (altAddr.addr == 0) {
        unbindAltAddr.port = altAddr.port;
    }
    if (verbose) {
        clog << "Running with on interface " << myAddr << " with alternate " << altAddr << endl;
    }

#if defined(WIN32)
    int pid=0;

    if (background) {
        cerr << "The -b background option does not work in windows" << endl;
        exit(-1);
    }
#else
    pid_t pid = 0;
#endif
    StunServerInfo info;
    bool ok;
    if (altAddr.addr != 0) {
        ok = stunInitServer(info, myAddr, altAddr, myMediaPort, true, verbose);
    } else {
        ok = stunInitServer(info, myAddr, unbindAltAddr, myMediaPort, false, verbose);
        info.altComAddr = unbindAltComAddr;
    }
    if (!ok) {
        return 1;
    }

    void (*serverLoop)(StunServerInfo &info, bool verboseStatistics, bool verbose) = NULL;
    if (useEpoll) {
        serverLoop = epollServerLoop;
    } else {
#ifdef HAVE_LIBEV
        serverLoop = evServerLoop;
#else
        serverLoop = selectServerLoop;
#endif
    }

    for (int i = 0; i < processNumber; ++i) {
        pid = fork();
        if (pid == 0) {  //child or not using background
            serverLoop(info, verboseStatistics, verbose);
            // Notreached
        }
    }
    if (!background) {
        serverLoop(info, verboseStatistics, verbose);
    }

    return 0;
}

#ifdef HAVE_LIBEV
static void sock_cb(struct ev_loop *defaultloop, ev_io *w, int revents) {
//    cout << "sock " << w->fd << " event" << endl;
    StunServerInfo &info = *static_cast<StunServerInfo *>(w->data);
    stunServerHandleMsg(info, w->fd, true, false);
}

void evServerLoop(StunServerInfo &info, bool verboseStatistics, bool verbose) {
    // use the default event loop unless you have special needs
    struct ev_loop *loop = EV_DEFAULT;

    ev_io myWatcher;
    ev_io_init(&myWatcher, sock_cb, info.myFd, EV_READ);
    myWatcher.data = &info;
    ev_io_start(loop, &myWatcher);

    ev_io altWatcher;
    if (info.altIpFd != INVALID_SOCKET) {
        ev_io_init(&altWatcher, sock_cb, info.altIpFd, EV_READ);
        altWatcher.data = &info;
        ev_io_start(loop, &altWatcher);
    }

    // now wait for events to arrive
    ev_run(loop, 0);

    // break was called, so exit
}

#endif

void selectServerLoop(StunServerInfo &info, bool verboseStatistics, bool verbose) {
    bool ok = true;
    int c = 0;
    while (ok) {
        ok = stunServerProcessNoRelay(info, verboseStatistics, verbose);
        c++;
        if (verbose && (c % 1000 == 0)) {
            clog << "*";
        }
    }
}

void setnonblocking(int sock) {
    int opts;
    opts = fcntl(sock, F_GETFL);
    if (opts < 0) {
        perror("fcntl(sock,GETFL)");
        exit(1);
    }
    opts = opts | O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opts) < 0) {
        perror("fcntl(sock,SETFL,opts)");
        exit(1);
    }
}

void epollServerLoop(StunServerInfo &info, bool verboseStatistics, bool verbose) {
    setnonblocking(info.myFd);
    setnonblocking(info.altPortFd);

    int epollfd = epoll_create(4);
    struct epoll_event ev;

    ev.data.fd = info.myFd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, info.myFd, &ev);

    if (info.altIpFd != INVALID_SOCKET) {
        setnonblocking(info.altIpFd);
        setnonblocking(info.altIpPortFd);
        ev.data.fd = info.altIpFd;
        ev.events = EPOLLIN | EPOLLET;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, info.altIpFd, &ev);
    }

    struct epoll_event actEvents[4];
    int c = 0;
    bool ok = true;
    while (ok) {
        int nfds = epoll_wait(epollfd, actEvents, 4, 1000);
        for (int i = 0; i < nfds; ++i) {
            Socket actFd = actEvents[i].data.fd;
            ok = stunServerHandleMsg(info, actFd, verboseStatistics, verbose);
        }
        c++;
        if (verbose && (c % 1000 == 0)) {
            clog << "*";
        }
    }
}

/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */

// Local Variables:
// mode:c++
// c-file-style:"ellemtel"
// c-file-offsets:((case-label . +))
// indent-tabs-mode:nil
// End:
