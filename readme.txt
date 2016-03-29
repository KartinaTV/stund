Running the STUN server:
------------------------

You need to have 2 IP addresses next to each other or 
have 2 hosts and each of them have 1 IP address to run the STUN server.

If you have 2 IP addresses next to each other on the same host, you can:
./stun_server -h primary-ip -a secondary-ip -b

If you have 2 hosts and each of them have 1 IP address, you can:
./stun_server -h primary-ip -u secondary-ip(pub)/secondary-ip(priv) -b

STUN server version 0.97
Usage: 
 ./stun_server [-v] [-h] [-e] [s] [-h IP_Address] [-a IP_Address] [-u IP_Address/IP_Address] 
    [-p port] [-o port] [-d port] [-m mediaport]
 
 If the IP addresses of your NIC are 10.0.1.150 and 10.0.1.151, run this program with
    ./stun_server -v  -h 10.0.1.150 -a 10.0.1.151

If the IP addresses of one of your host is 172.16.10.55. There is another host, of which the NIC
is 172.16.10.59, and there is a public IP address 123.45.32.76 being bind to it, run this
program with
    ./stun_server -b  -h 172.16.10.55 -u 123.45.32.76/172.16.10.59

 STUN servers need two IP addresses and two ports, these can be specified with:
  -e whether receive msg with epoll mechanism
  -s whether display statistics logs
  -h sets the primary IP
  -a sets the secondary IP
  -u sets the unbind secondary IP
  -p sets the primary port and defaults to 3478
  -o sets the secondary port and defaults to 3479
  -d sets the unbind secondary communication port and defaults to 3481
  -b makes the program run in the background
  -m sets up a STERN server starting at port m
  -s runs in verbose statistics mode
  -v runs in verbose mode

