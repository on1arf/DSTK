/* multicast.h */

// multicast.h: functions related to opening a multicast-stream 

// copyright (C) 2010 Kristoff Bonne ON1ARF
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
// 19 May 2011: version 0.0.1. Initial release

// open and join multicast: does what its name implies
// returns socket

#define MULTICASTBUFFSIZE 65536

// ############################
// ############################
// # OPEN AND JOIN MULTICAST ##
// ############################

int open_and_join_mc (char * multicastaddress, int port, int verboselevel) {

///////////////////////
// data definition part

// local vars
int sock; // socket, will be returned at the end
struct sockaddr_in6 localsa; // socket address structure
struct in6_addr multicast_ia6; // structure containing multicast ipv6 address

// multicast join structure
struct ipv6_mreq multicastrequest;

// to set multicast option
int optval;
socklen_t optval_len = sizeof(int);

// other vars
int ret;
int yes=1;


///////////////////
// code starts here
///////////////////

// init socket address structure
localsa.sin6_family=AF_INET6;
localsa.sin6_port=htons(port);
localsa.sin6_flowinfo=0; // flows not used here
localsa.sin6_scope_id=0; // we listen on all interfaces for multicast
memset(&localsa.sin6_addr,0,sizeof(struct in6_addr)); // address "::" (all 0) -> listen on all ip-addresses

// create socket
sock=socket(AF_INET6,SOCK_DGRAM,0);
if (sock < 0) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Error: could not create socket! \n");
	}; // end if
	return(-1);
}; // end if

// make socket ipv6-only.
// this way, there will not be a conflict with any other application running on the repeater that
// would use the same UDP port (but would be ipv4 only)
ret=setsockopt(sock,IPPROTO_IPV6, IPV6_V6ONLY, (char *)&yes,sizeof(int));
if (ret == -1) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Error: could not make socket ipv6 only! \n");
	}; // end if
	return(-1);
}; // end if

// set "reuse" option
ret=setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&yes,sizeof(int));
if (ret == -1) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Error: could not set setsockopt reuse\n");
	}; // end if
	return(-1);
}; // end if

// set socket receive buffer
// Based on code originally writen by tmouse, July 2005
// http://cboard.cprogramming.com/showthread.php?t=67469
optval_len = sizeof(int);

optval=MULTICASTBUFFSIZE;
ret=setsockopt(sock,SOL_SOCKET,SO_RCVBUF, (char *) &optval, sizeof(optval));
if (ret != 0) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Error: could not set setsockopt receivebuffer \n");
	}; // end if
}; // end if

// now retrieve value and see if it is indeed what we have set
ret=getsockopt(sock,SOL_SOCKET,SO_RCVBUF, (char *) &optval, &optval_len);
if (ret != 0) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Error: could not get setsockopt receivebuffer \n");
	}; // end if
}; // end if

if (optval != MULTICASTBUFFSIZE) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Warning: setsockopt receivebuffer did not succeed. Buffer is %d.\n",optval);
	}; // end if
}; // end if



// bind to multicast port
ret=bind(sock,(struct sockaddr*) &localsa, sizeof(struct sockaddr_in6));
if (ret != 0) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Error: could not bind to multicast port\n");
	}; // end if
	return(-1);
}; // end if

// join ipv6 multicast group

// convert ipv6-address into binary format
ret=inet_pton(AF_INET6,multicastaddress,(void *)&multicast_ia6);
if (ret != 1) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Error: could not convert %s info valid ipv6 address. Exiting!\n",multicastaddress);
	}; // end if
	return(-1);
}; // end if


// fill in multicastrequest structure
memcpy(&multicastrequest.ipv6mr_multiaddr, &multicast_ia6, sizeof(multicastrequest.ipv6mr_multiaddr));
multicastrequest.ipv6mr_interface=0; // any interface

// do "join" request
ret=setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *) &multicastrequest, sizeof(multicastrequest));
if (ret != 0) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Error: could not make ipv6 multicast join request\n");
	}; // end if
	return(-1);
}; // end if


// OK, all went well, return the socket
return(sock);

}; // end function open_and_join_mc



// ############################
// ############################
// # OPEN FOR MULTICAST  OUT ##
// ############################

int open_for_multicast_out (int verboselevel) {
int udpsd;
char loopch=0;

udpsd=socket(AF_INET6,SOCK_DGRAM,0);

if (udpsd < 0) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Error: could not create udp socket!\n");
	}; // end if
	return(-1);
}; // end if

// set socket options for IP multicast
if (setsockopt(udpsd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
	if (verboselevel >= 1) {
		fprintf(stderr,"Error: could not set IP_MULTICAST_LOOP socket options! Exiting!\n");
	}; // end if
	close(udpsd);
	return(-1);
}; // end if

// success: return socket
return (udpsd);

}; // end function open_for_multicast_out


