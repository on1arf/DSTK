/* cap2rpc.c */



// cap2rpc: capture DVframes from ethernet-interface between gateway-server
// and repeater-controller and re-broadcast as multicast DSTK frames
// This application will rebroadcast the packages from the IP-layer
// upwards. (RPCIP format)
// This application needs to run with root priviledges

// copyright (C) 2011 Kristoff Bonne ON1ARF
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
// 14 jun. 2011: version 0.1.0: Initial release

// selective compiling per application
#define CAP2RPC

#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <netinet/ip.h>
#include <netinet/udp.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <string.h>

#include <unistd.h>

// needed for gettimeofdat (used with srandom)
#include <sys/time.h>

// global defines
#include "include/globaldef.h"

// global structures for DSTAR SwitchMatrix Tookit
#include "include/dstk.h"

// Substream structures for the TYPE_RPC data
#include "include/dss_rpc.h"

// structures and data specific for cap2rpc
#include "cap2rpc.h"

// functions for multicasting
#include "include/multicast.h"


//////////////////////////////////////////////////////////////////////////
// configuration part // // configuration part // // configuration part //
//////////////////////////////////////////////////////////////////////////

// default destination address, interface and port
char * default_d_ipaddress="ff11::4100";
int default_d_port=4100;

// default parsefilter. This works for the default setup of an
// i-com based repeater

char * default_parsefilter="host 172.16.0.1 and udp port 20000";
char * default_interface="eth1";

//////////////////////////////////////////////////////////////////////////
// end configuration part. Do not change below unless you               //
// know what you are doing                                              //
//////////////////////////////////////////////////////////////////////////


// help and usage
// should be included AFTER setting up the default values
#include "include/helpandusage.h"


int main (int argc, char * argv[]) {

// input
char * parsefilter;
char * interface;

// output
unsigned char sendbuffer[ETHERNETMTU];
dstkheader_str * dstkheader;
void * dstkdata;

char * d_ipaddress;
int d_port;

// verbose
int verboselevel=0;

// outgoing udp stream
int sock_out;
struct sockaddr_in6 MulticastOutAddr6;
struct sockaddr_in MulticastOutAddr4;
int v4orv6_out=-1; // 0=ipv4, 1=ipv6
char loopch=0;


// headers
struct ethhdr * etherhead; // ethernet header
struct dss_rpc_header * rpchead; // D-STAR SubStream RPC header: IP + UDP + RPC HEADER

// everything pcap related
pcap_t * pcaphandle;
char errbuf[PCAP_ERRBUF_SIZE];
struct bpf_program fp;
struct pcap_pkthdr * header;
const u_char *packet;


// some other vars
int ret;
int timeouts;
int packetsequence;
int len;
int udp_len;
int rpcdata_len;
int paramloop;

// time structures: used with srandom
struct timeval now;
struct timezone tz;

// ////// data definition done ///

// main program starts here ///
// part 1: initialise vars and check cli-arguments
d_ipaddress=default_d_ipaddress;
d_port=default_d_port;
interface=default_interface;
parsefilter=default_parsefilter;


// CLI option decoding
// format: cap2rpc [-v] [-i interface] [-p "parsefilter"] [-di ipaddress ] [ -dp port] 
// format: cap2rpc [-V]
// format: cap2rpc [-h]

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
	} else if (strcmp(thisarg,"-di") == 0) {
		// -di = DESTINATION ipaddress
		if (paramloop+1 < argc) {
			paramloop++;
			d_ipaddress=argv[paramloop];
		}; // end if
	} else if (strcmp(thisarg,"-dp") == 0) {
		// -dp = DESTINATION port
		if (paramloop+1 < argc) {
			paramloop++;
			d_port=atoi(argv[paramloop]);
		}; // end if
	} else if (strcmp(thisarg,"-i") == 0) {
		// -i = interface
		if (paramloop+1 < argc) {
			paramloop++;
			interface=argv[paramloop];
		}; // end if
	} else if (strcmp(thisarg,"-p") == 0) {
		// -p = parsefilter
		if (paramloop+1 < argc) {
			paramloop++;
			parsefilter=argv[paramloop];
		}; // end if
	}; // end elsif ... if
}; // end for


// sanity checks


////// 
// main program starts here

// init vars and buffers
dstkheader = (dstkheader_str *) sendbuffer;
dstkdata = (void *) sendbuffer + sizeof(dstkheader_str);

// reset all values to 0
memset(dstkheader,0,sizeof(dstkheader_str));

// fill in fixed parts
dstkheader->version=1;
// only one single subframe in a DTAK frame: set next-header to 0
dstkheader->flags=0 | DSTK_FLG_LAST;

dstkheader->type=htonl(TYPE_RPC);
dstkheader->origin=htons(ORIGIN_LOC_RPC);

// streamid is random
gettimeofday(&now,&tz);
srandom(now.tv_usec);

dstkheader->streamid1=random();
dstkheader->streamid2=0;


// open pcap handle for live snooping on interface eth1
pcaphandle = pcap_open_live(interface,PKT_BUFSIZE,0,500,errbuf);

if (pcaphandle == NULL) {
	fprintf(stderr,"could not open device %s: %s\n",interface,errbuf);
	exit(-1);	
}; // end if

// pcap filter

if (pcap_compile(pcaphandle, &fp, parsefilter, 0, 0) == -1) {
	fprintf(stderr,"could not compile filter %s: %s\n",parsefilter,pcap_geterr(pcaphandle));
	exit(-1);
}; // end if

if (pcap_setfilter(pcaphandle, &fp) == -1) {
	fprintf(stderr, "Couldn't install filter %s: %s\n", parsefilter, pcap_geterr(pcaphandle));
	exit(-1);
}


// packets are send to multicast address, UDP port 40000 or 40001
MulticastOutAddr4.sin_family = AF_INET; 
MulticastOutAddr4.sin_port = htons((unsigned short int) d_port);

MulticastOutAddr6.sin6_family = AF_INET6; 
MulticastOutAddr6.sin6_scope_id = 1;
MulticastOutAddr6.sin6_port = htons((unsigned short int) d_port);


// try to resolv for ipv4
ret=inet_pton(AF_INET,d_ipaddress,(void *)&MulticastOutAddr4.sin_addr);
if (ret == 1) {
	v4orv6_out=0;
} else {
	// coulf not convert address from text into valid address, try again in ipv6
	ret=inet_pton(AF_INET6,d_ipaddress,(void *)&MulticastOutAddr6.sin6_addr);

	if (ret != 1) {	
		fprintf(stderr,"Error: could not convert %s into valid address. Exiting\n",d_ipaddress);
		exit(1);
	}; // end if

	v4orv6_out=1;
}; // end if


// open socket for outgoing UDP
if (v4orv6_out) {
	// ipv6
	sock_out=socket(AF_INET6,SOCK_DGRAM,0);
} else {
	// ipv4
	sock_out=socket(AF_INET,SOCK_DGRAM,0);
}; // end else - if

if (sock_out <= 0) {
	fprintf(stderr,"Could not create socket for multicast UDP out! Exiting!\n");
	exit(-1);
}; // end if


// set socket options for IP multicast
// should be ignored for unicast traffic
if (setsockopt(sock_out, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
	fprintf(stderr,"Error: could not set IP_MULTICAST_LOOP socket options! Exiting!\n");
	exit(-1);
}; // end if



int fd = pcap_get_selectable_fd(pcaphandle);

if (fd < 0) {
	fprintf(stderr,"Error: could not get file description for select\n");
	exit(-1);
}; // end if


timeouts=0;
packetsequence=0;

// endless loop
while (forever) {

	// blocking read with timeout: so we use a select
	fd_set rfds;
	struct timeval tv;


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

	// ret=0, we hit a timeout
	if (ret == 0) {
		timeouts++;

		// we stop if we have not received any data during 10 minutes
		if (timeouts > 600) {
			break;
		}; // end if

//fprintf(stderr,"timeout! \n");
		continue;
	}; // end if

	// reset timeouts
	timeouts=0;
	// we have received data, let's check if it is something we can use
	ret = pcap_next_ex( pcaphandle, &header, &packet);

	if (ret == -1) {
		fprintf(stderr,"Error: pcap_next_ex returns an error: %s\n",pcap_geterr(pcaphandle));
		exit(-1);
	}; // end if

	len=header->len;

	// sanity check:
	// the received packet should be at least the size of an ethernet header
	// + the size of the DSTAR_RPC header


	// "38" = sizeof (struct ethhdr) + sizeof(struct dss_rpc_header)
	// however, due to the 32 bit allignments the "sizeof" functions return 40
	if (len < 38) {
		if (verboselevel >= 1) {
			fprintf(stderr,"Error: packet not large enough to hold ethernet header + RPC header\n");
		}; // end if
		continue;
	}; // end if

	// extract ethernet header
	etherhead = (struct ethhdr *) packet;

	// is it IP?
	if (ntohs(etherhead->h_proto) != ETH_P_IP) {
		// not IP
		if (verboselevel > 1) {
			fprintf(stderr,"Error: received packet is not an IP packet\n");
		}; // end if


		continue;
	}; // end if


	// next part of the packet is IP. So it should hold at least the
	// TYPE_RPC header structure
	rpchead = (struct dss_rpc_header *) (packet + sizeof(struct ethhdr));


	// is it UDP?
	if (rpchead->ipheader.protocol != IPPROTO_UDP) {
		// not UDP
		if (verboselevel > 1) {
			fprintf(stderr,"Error: received packet is not a UDP packet\n");
		}; // end if
		continue;
	}; // end if

	udp_len = ntohs(rpchead->udpheader.len);

	// We do not check on UDP port-number as that has already been
	// filtered out by the filter


	// First 4 octets should be 'DSTR'
	// cast 2nd argument to (char *) to avoid warning about signedness
	if (strncmp("DSTR", (char *)rpchead->rpcheader.dstar_id,4) != 0) {
		// not a DSTAR packet
		if (verboselevel > 1) {
			fprintf(stderr,"Error: missing DSTR signature\n");
		}; // end if
		continue;
	}; // end if


	rpcdata_len = ntohs(rpchead->rpcheader.dstar_data_len);

	// sizecheck: the full rest of the UDP packet should be large enough to contain the rest of the D-STAR packet
	if (udp_len < (rpcdata_len + sizeof(struct dstar_rpc_header) + sizeof (struct udphdr))) {
		// check fails
		fprintf(stderr,"Error: packet size of DSTAR packet does not match size of UDP frame!\n");
		continue;
	}; // end if

	// set packetsequence
	dstkheader->sequence.seq32.seq1=htons(packetsequence);
	dstkheader->sequence.seq32.seq2=0;
	packetsequence++;

	// copy data
	memcpy(dstkdata,rpchead,rpcdata_len+sizeof(struct dss_rpc_header));

	// set size
	dstkheader->size=htons(rpcdata_len+sizeof(struct dss_rpc_header));

	// send DSTAR frame
	// size if udp_len (data + UDP header) + size of IP-header
	if (v4orv6_out) {
		// ipv6
		ret=sendto(sock_out,dstkheader,udp_len+sizeof(struct iphdr)+sizeof(dstkheader_str),0,(struct sockaddr *) &MulticastOutAddr6, sizeof(MulticastOutAddr6));
	} else {
		// ipv4
		ret=sendto(sock_out,dstkheader,udp_len+sizeof(struct iphdr)+sizeof(dstkheader_str),0,(struct sockaddr *) &MulticastOutAddr4, sizeof(MulticastOutAddr4));
	}; // end if

	if (ret < 0) {
		if (verboselevel >= 1) {
			fprintf(stderr,"Warning: DSTK packet could not be send. Error: %d (%s)!\n",errno,strerror(errno)); 
		}; // end if
	}; // end if



}; // end while

return(0);

}; // end main
