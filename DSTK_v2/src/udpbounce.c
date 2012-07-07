/* udpbounce.c */


// udpbounce: receive UDP traffic (unicast or multicast) and rebroadcast
// as a UDP packet (unicast or multicast).
// The goal is to transport DSTK (multicast) traffic from one host
// to another, using UDP unicast

// copyright (C) 2012 Kristoff Bonne ON1ARF
/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// release info:
// 03 jul. 2012: version 0.0.1: Initial release

// selective compiling per application
#define UDPBOUNCE

#define VERSION "0.0.1"

// for fprintf
#include <stdio.h>

// sockets
#include <netinet/in.h>

// for memset and memcpy
#include <string.h>

// for exit
#include <stdlib.h>

// for close
#include <unistd.h>

// for errno
#include <errno.h>

// for inet_pton
#include <arpa/inet.h>

// global vars + definitions
#include "include/globaldef.h"

// functions for multicasting
#include "include/multicast.h"


//////////////////////////////////////////////////////////////////////////
// configuration part // // configuration part // // configuration part //
//////////////////////////////////////////////////////////////////////////

// default destination address, interface and port
int default_s_port=0;
char * default_d_ipaddress=NULL;
int default_d_port=0;
int default_timeout=3600; // quit program if no data received in one hour


//////////////////////////////////////////////////////////////////////////
// end configuration part. Do not change below unless you               //
// know what you are doing                                              //
//////////////////////////////////////////////////////////////////////////


// help and usage
// should be included AFTER setting up the default values
#include "include/helpandusage.h"


int main (int argc, char * argv[]) {

// local vars
unsigned char etherbuffer[ETHERNETMTU];
int buffersize=0;
int fd=-1; // file descriptor
int paramloop;
int ret;
int v4orv6_mc=-1;
int v4orv6_out=-1;
char loopch=0;

int yes=1;

int optval;
socklen_t optval_len;

// input
int s_port=0;
int timeout=0;

// multicast
int multicast=0;
char * multicastjoin;
struct ip_mreq mcreq4;
struct ipv6_mreq mcreq6;


// output
char * d_ipaddress;
int d_port;

// verbose
int verboselevel=0;

// parameters given?
int i_source=0;
int i_dest=0;

// outgoing udp stream
int sock_out;
struct sockaddr_in OutAddr4;
struct sockaddr_in6 OutAddr6;
struct sockaddr_in6 InAddr6;

struct in_addr multicast_ia4; // multicast
struct in6_addr multicast_ia6; // multicast

// vars for select (read input with timeout)
int timeout_received;
fd_set rfds;
struct timeval tv;

// ////// data definition done ///

// main program starts here ///
// part 1: initialise vars and check cli-arguments
s_port=default_s_port;
d_ipaddress=default_d_ipaddress;
d_port=default_d_port;
timeout=default_timeout;


// CLI option decoding
// format: udpbounce [-v] [-t timeout] [-mj multicast-ipaddress] -s port  -d ipaddress port
// format: cap2rpc [-V]
// format: cap2rpc [-h]

// no parameters is same as "help"
if (argc == 1) {
	help(argv[0]);
	exit(0);
}; // end if

// parse CLI options
for (paramloop=1;paramloop<argc;paramloop++) {
	char * thisarg=argv[paramloop];

	if (strcmp(thisarg,"-V") == 0) {
		// -V = version
		fprintf(stderr,"%s version %s\n",argv[0],VERSION);
		exit(0);
	 } else if (strcmp(thisarg,"-h") == 0) {
		// -h = help
		help(argv[0]);
		exit(0);
	} else if (strcmp(thisarg,"-v") == 0) {
		// -v = verbose
		verboselevel++;
	} else if (strcmp(thisarg,"-d") == 0) {
		// -d = DESTINATION ipaddress + port
		if (paramloop+2 < argc) {
			paramloop++;
			d_ipaddress=argv[paramloop];
			paramloop++;
			d_port=atoi(argv[paramloop]);
			i_dest=1;
		}; // end if
	} else if (strcmp(thisarg,"-mj") == 0) {
		// -mj = multicast join
		if (paramloop+1 < argc) {
			paramloop++;
			multicastjoin=argv[paramloop];
			multicast=1; // set "multicast" flag
		}; // end if
	} else if (strcmp(thisarg,"-s") == 0) {
		// -s = source port
		if (paramloop+1 < argc) {
			paramloop++;
			s_port=atoi(argv[paramloop]);
			i_source=1;
		}; // end if
	} else if (strcmp(thisarg,"-t") == 0) {
		// -t = time out
		if (paramloop+1 < argc) {
			paramloop++;
			timeout=atoi(argv[paramloop]);
		}; // end if
	}; // end elsif ... if
}; // end for


// sanity checks
if ((i_source + i_dest) < 2) {
	fprintf(stderr,"Error: Missing parameters, need at least source port, destination port and destination ip-address\n");
	usage(argv[0]);
	exit(-1);
}; // end if

if (s_port <= 0) {
	fprintf(stderr,"Error: Source UDP port not defined or less then 0, got %d\n",s_port);
	exit(-1);
}; // end if

if (d_port <= 0) {
	fprintf(stderr,"Error: Destination UDP port not defined or less then 0, got %d\n",d_port);
	exit(-1);
}; // end if


if (!d_ipaddress) {
	fprintf(stderr,"Error: Destination IP address not defined\n");
	exit(-1);
}; // end if

if (timeout < 0) {
	fprintf(stderr,"Error: Timeout value should be at greater then 0, got %d\n",timeout);
	exit(-1);
}; // end if



////// 
// main program starts here

// init vars and buffers

// is destination ipv4 or ipv6?
OutAddr4.sin_family = AF_INET;
OutAddr4.sin_port = htons((unsigned short int) d_port);

OutAddr6.sin6_family = AF_INET6; 
OutAddr6.sin6_scope_id = 1;
OutAddr6.sin6_port = htons((unsigned short int) d_port);

// try to resolv 
ret=inet_pton(AF_INET,d_ipaddress,(void *)&OutAddr4.sin_addr);

if (ret == 1) {
	v4orv6_out=0;
} else {
	// Could not convert address in text into valid address, try again in ipv6
	ret=inet_pton(AF_INET6,d_ipaddress,(void *)&OutAddr6.sin6_addr);

	// stop if not valid for ipv4 or ipv6
	if (ret != 1) {
		fprintf(stderr,"Error: could not convert %s into valid address. Exiting\n",d_ipaddress);
		exit(-1);
	}; // end if

	v4orv6_out=1;
}; // end if


// /// open socket for outgoing UDP
if (v4orv6_out) {
	// ipv6
	sock_out=socket(AF_INET6,SOCK_DGRAM,0);
} else {
	// ipv4
	sock_out=socket(AF_INET,SOCK_DGRAM,0);
}; // end else - if 

if (sock_out <= 0) {
	fprintf(stderr,"Could not create create udp socket!\n");
	exit(-1);
}; // end if

// set socket options for IP multicast
// should be ignored for unicast traffic
if (setsockopt(sock_out, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
	fprintf(stderr,"Error: could not set IP_MULTICAST_LOOP socket options! Exiting!\n");
	exit(-1);
}; // end if



// open inbound UDP socket

// we create an ipv6 socket, as this can receive both ipv4 and ipv6 traffic
fd=socket(AF_INET6,SOCK_DGRAM,0);

if (fd < 0) {
	fprintf(stderr,"Error: could not create inbound UDP socket: %d (%s)!\n",errno,strerror(errno)); 
	exit(-1);
}; // end if

// init vars
InAddr6.sin6_family=AF_INET6;
InAddr6.sin6_port=htons(s_port);
InAddr6.sin6_flowinfo=0; // flows not used
InAddr6.sin6_scope_id=0; // we listen on all interfaces
memset(&InAddr6.sin6_addr,0,sizeof(struct in6_addr)); // address = "::" (all 0) -> we listen
																				// to all ip-addresses of host

// set "reuse" option
ret=setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(char *)&yes,sizeof(int));
if (ret == -1) {
	fprintf(stderr,"Error: could not set setsockopt reuse\n");
	return(-1);
}; // end if

// set socket receive buffer
// Based on code originally writen by tmouse, July 2005
// http://cboard.cprogramming.com/showthread.php?t=67469
optval_len = sizeof(int);

optval=MULTICASTBUFFSIZE;

ret=setsockopt(fd,SOL_SOCKET,SO_RCVBUF, (char *) &optval, optval_len);
if (ret != 0) {
	fprintf(stderr,"Error: could not set setsockopt receivebuffer \n");
	exit(-1);
}; // end if

// now retrieve value and see if it is indeed what we have set
ret=getsockopt(fd,SOL_SOCKET,SO_RCVBUF, (char *) &optval, &optval_len);
if (ret != 0) {
	fprintf(stderr,"Error: could not get setsockopt receivebuffer \n");
	exit(-1);
}; // end if

if (optval != MULTICASTBUFFSIZE) {
	fprintf(stderr,"Warning: setsockopt receivebuffer did not succeed. Buffer is %d, requested %d.\n",optval,MULTICASTBUFFSIZE);
}; // end if

ret=bind(fd, (struct sockaddr *)&InAddr6, sizeof(InAddr6));
if (ret < 0) {
	fprintf(stderr,"Error: bind for UDP failed: %d (%s)\n",errno,strerror(errno));
	return(-1);
}; // end if



// MULTICAST JOIN. Depends on ipv4 or ipv6
if (multicast) {
	// determine if multicast address is ipv4 or ipv6
	ret=inet_pton(AF_INET,multicastjoin, (void*) &multicast_ia4);

	if (ret == 1) {
		// ipv4
		v4orv6_mc=0;
	} else {
		// try again with ipv6
		ret=inet_pton(AF_INET6,multicastjoin, (void*) &multicast_ia6);

		if (ret == 1) {
			// ipv6
			v4orv6_mc=1;
		};
	};

	if (v4orv6_mc < 0) {
		fprintf(stderr,"Error: Multicast join address is not recognised as either ipv4 or ipv6 \n");
		exit(-1);
	}; // end if

	if (v4orv6_mc) {
		// ipv6

		// fill in multicastrequest structure
		memcpy(&mcreq6.ipv6mr_multiaddr, &multicast_ia6, sizeof(mcreq6.ipv6mr_multiaddr));
		mcreq6.ipv6mr_interface=0; // any interface

		// do "join" request
		ret=setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *) &mcreq6, sizeof(mcreq6));
		if (ret != 0) {
			fprintf(stderr,"Error: could not make ipv6 multicast join request\n");
			return(-1);
		}; // end if

	} else {
		// ipv4
		// fill in multicastrequest structure
		memcpy(&mcreq4.imr_multiaddr, &multicast_ia4, sizeof(mcreq4.imr_multiaddr));
		mcreq4.imr_interface.s_addr=htonl(INADDR_ANY); // any interface

		// do "join" request
		ret=setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mcreq4, sizeof(mcreq4));
		if (ret != 0) {
			fprintf(stderr,"Error: could not make ipv6 multicast join request\n");
		}; // end if

	}; // end if

}; // end if (multicast?)

timeout_received=0;

// endless loop
while (forever) {

	// blocking read with timeout: so we use a select

	// set timeout to 1 second
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	ret= select(fd+1,&rfds,NULL,NULL,&tv); 

	if (ret < 0) {
		fprintf(stderr,"Error: select fails (errno=%d)\n",errno);
		exit(-1);
	}; // end if

	// ret=0, we hit a timeout. Only check if timeout value set
	if ((ret == 0) && (timeout > 0)) {
		timeout_received++;

		// we stop if we have not received any data during 10 minutes
		if (timeout_received > timeout) {
			fprintf(stderr,"timeout! \n");
			break;
		}; // end if

		continue;
	}; // end if

	// reset timeout_received
	timeout_received=0;


	// now get data
	buffersize=recvfrom(fd, etherbuffer,ETHERNETMTU,0, NULL, 0);

	// resend frame
	if (buffersize > 0) {
		// is there something to send?
		if (v4orv6_out) {
			// ipv6
			ret=sendto(sock_out,etherbuffer, buffersize,0,(struct sockaddr *) &OutAddr6, sizeof(OutAddr6));
		} else {
			// ipv4
			ret=sendto(sock_out,etherbuffer, buffersize,0,(struct sockaddr *) &OutAddr4, sizeof(OutAddr4));
		}; // end else - if

		if (ret < 0) {
			fprintf(stderr,"outbound packet could not be send. Error: %d (%s)!\n",errno,strerror(errno)); 
		}; // end if
	}; // end if (there is data to send)



}; // end while

return(0);

}; // end main
