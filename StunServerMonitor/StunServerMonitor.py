import argparse
import random
import socket
import sys
from threading import Timer
import time
import urllib2
from xml.etree import ElementTree

import stun

RefreshUrl = "http://172.16.10.71/set?stream_id=stuntest"

def request_url(url):
    req = urllib2.Request(url)
    res = urllib2.urlopen(req)

    content = res.read()
    return content

def make_argument_parser():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    parser.add_argument(
        '-d', '--debug', action='store_true',
        help='Enable debug logging'
    )
    parser.add_argument(
        '-H', '--stun-host',
        help='STUN host to use'
    )
    parser.add_argument(
        '-P', '--stun-port', type=int,
        default=stun.DEFAULTS['stun_port'],
        help='STUN host port to use'
    )
    parser.add_argument(
        '-i', '--source-ip',
        default=stun.DEFAULTS['source_ip'],
        help='network interface for client'
    )
    parser.add_argument(
        '-p', '--source-port', type=int,
        default=stun.DEFAULTS['source_port'],
        help='port to listen on for client'
    )

    parser.add_argument('--version', action='version', version=stun.__version__)

    return parser


def parse_stunservers():
    file_path = r"stunservers.xml"
    root = ElementTree.parse(file_path)
    server_lst = root.getiterator("stunserveraddr")
    servers = []
    for server in server_lst:
        servers.append(server.text)
    return servers

def main():
    options = make_argument_parser().parse_args()
    stunHost = options.stun_host

    stunservers = parse_stunservers()

    source_ip = "0.0.0.0"
    stun._initialize()
    TimerInterval = 50
    i = 1
    for serveraddr in stunservers:
        source_port = random.randint(40000, 50000)

        socket.setdefaulttimeout(2)
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((source_ip, source_port))

        def delayrun(sock, source_ip, source_port, serveraddr, refreshurl):
            try:
                buf, addr = stun.test_get_ip_info(sock, source_ip, source_port, serveraddr)
                if addr is not None:
                    try:
                        refresh_ret = request_url(refreshurl)
                        print("Server addr = " + str(addr) + " local port " + str(source_port) + " refresh url = "
                              + str(refreshurl) + ", refreshRet = " + str(refresh_ret.strip()))
                    except urllib2.URLError, e:
                        print("URLError = " + str(e))
                args = [sock, source_ip, source_port, serveraddr, refreshurl]
                t = Timer(TimerInterval, delayrun, args)
                t.start()
            except KeyboardInterrupt, e:
                sys.exit()
        refresh_url = RefreshUrl + "&ip=" + serveraddr
        args = [s, source_ip, source_port, serveraddr, refresh_url]
        t = Timer(i, delayrun, args)
        t.start()
        ++ i

    while True:
        time.sleep(150)

if __name__ == '__main__':
    main()
