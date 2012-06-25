/* rpc2amb.c */

// rpc2amb: join 'RPCIP'-stream, and multicast out as one 'AMB'-stream
// per module
// Usage: rpc2amb [-v ] [-si ipaddress ] [-sp port] [-di ipaddress] [-dp port ] [-dpi portincease] module ... [module]

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
// 14 jun. 2011: version 0.0.1. Initial release

#define RPC2AMB

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>

// sockets
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

// posix threads + inter-process control
#include <pthread.h>
#include <signal.h>

// posix interupt timers
#include <time.h>

// for flock
#include <sys/file.h>

// error numbers
#include <errno.h>

// header-files for ip and udp
#include <netinet/ip.h>
#include <netinet/udp.h>

// change this to 1 for debugging
#define DEBUG 0

// dstk.h: global datastructures for DSTK toolkit
#include "include/dstk.h"

// dstkfunctions.h: functions for the DSTK toolkit
#include "include/dstkfunctions.h"

// D-STAR SubStream structures for TYPE_RPC and TYPEGRP_AMB
#include "include/dss_rpc.h"
#include "include/dss_amb.h"

// rpc2amb.h: data structures + global data special for this program
#include "rpc2amb.h"


//  multicast functions
#include "include/multicast.h"

// defines for timed interrupts
#define CLOCKID CLOCK_REALTIME


//////////////////////////////////////////////////////////////////////////
// configuration part // // configuration part // // configuration part //
//////////////////////////////////////////////////////////////////////////
// ipaddress and port on which we listen
char * default_s_ipaddress="ff11::4100";
int default_s_port=4100;

// ipaddress and port on which we broadcast
char * default_d_ipaddress="ff11::4200";
int default_d_port=4200; 
int default_d_portincr=1;

// my own IP-address (facing repeater-controller). Used to determine
// direction
char * default_myipaddress="172.16.0.20";

// end configuration part. Do not change below unless you
// know what you are doing

// help and usage
// should be included AFTER setting up the default values
#include "include/helpandusage.h"

// functions defined further down
void funct_heartbeat (int sig); 


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Main program // Main program // Main program // Main program //////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


int main(int argc,char** argv) {
// vars
// loops
int paramloop;
int loop;

// some miscellaneous vars:
int ret;

// verboselevel
int verboselevel;

// active modules
int modactive[3];

// vars for source and destination ipaddresses and ports
char *s_ipaddress, *d_ipaddress;
int s_port, d_port, d_portincr;
struct in_addr myaddr_bin;
char * myipaddress;

// vars related to receiving data
unsigned char receivebuffer[ETHERNETMTU];
int packetsize;
int udp_packetsize;
int dstar_data_len;

// vars to process streams
int packetsequence;
int direction;
int dvstreamid;
int streamid_instream;

// vars related to sending data
unsigned char sendbuffer[ETHERNETMTU];
struct sockaddr_in6 MulticastOutAddr;

// vars to deal with DSTK frames
dstkheader_str * dstkhead_in, *dstkhead_out;
void * dstkdata;

int othersize;
// define memory for "other". Define it the same size of a
// received packet minus the smallest possible packet
// (which is a AMBE DV frame)
unsigned char * other[ETHERNETMTU - sizeof(struct dstar_fullframe_dvdata)];

// dealing with IP, UDP and DSTAR headers
struct iphdr * iphead;
struct udphdr * udphead;
struct dstar_rpc_header * dstar_rpc_head;
unsigned char * dstardata;

struct dstar_dv_artheader * dv_artheader;
struct dstar_header2 * dv_header;
struct dstar_dv_data * dv_data;
uint8_t this_sequence;

char thismodule_c;
int thismodule_i;

int activestatus[3];
int activedirection[3];
u_short activestreamid[3];

// networking vars
int sock_in, sock_out;


// vars for timed interrupts
struct sigaction sa;
struct sigevent sev;
timer_t timerid;
struct itimerspec its;

// ////// data definition done ///


// main program starts here ///
// part 1: initialise vars and check cli-arguments

verboselevel=0;
s_ipaddress=default_s_ipaddress; s_port=default_s_port;
d_ipaddress=default_d_ipaddress; d_port=default_d_port;
d_portincr=default_d_portincr;
myipaddress=default_myipaddress;

modactive[0]=0; modactive[1]=0; modactive[2]=0;

activestatus[0]=0; activestatus[1]=0;activestatus[2]=0;

global.inbound_timeout[0]=0; global.inbound_timeout[1]=0; 
global.inbound_timeout[2]=0; 

// CLI option decoding
// format: rpc2amb [-v ] [-myip ipaddress] [-si ipaddress ] [-sp port] [-di ipaddress] [-dp port ] [-dpi portincease] module ... [module]

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
	} else if (strcmp(thisarg,"-myip") == 0) {
		// -myip = my ipaddress on interface facing RPC
		if (paramloop+1 < argc) {
			paramloop++;
			myipaddress=argv[paramloop];
		}; // end if
	} else if (strcmp(thisarg,"-si") == 0) {
		// -si = SOURCE ipaddress
		if (paramloop+1 < argc) {
			paramloop++;
			s_ipaddress=argv[paramloop];
		}; // end if
	} else if (strcmp(thisarg,"-sp") == 0) {
		// -si = SOURCE port
		if (paramloop+1 < argc) {
			paramloop++;
			s_port=atoi(argv[paramloop]);
		}; // end if
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
	} else if (strcmp(thisarg,"-dpi") == 0) {
		// -dp = DESTINATION port increase
		if (paramloop+1 < argc) {
			paramloop++;
			d_portincr=atoi(argv[paramloop]);
		}; // end if
	} else {
		// modules: can be 'a' up to 'c'
		if ((thisarg[0] == 'a') || (thisarg[0] == 'A')) {
			modactive[0]=1;
		} else if ((thisarg[0] == 'b') || (thisarg[0] == 'B')) {
			modactive[1]=1;
		} else if ((thisarg[0] == 'c') || (thisarg[0] == 'C')) {
			modactive[2]=1;
		} else {
			fprintf(stderr,"Warning: unknown module %c\n",thisarg[0]);
			usage(argv[0]);
		}; // end elsif (...) - if
	}; // end elsif - elsif - elsif - if
}; // end for


// we should have at least one active module
if (!(modactive[0] | modactive[1] | modactive[2])) {
	fprintf(stderr,"Error: At least one active module expected! \n");
	usage(argv[0]);
	exit(-1);
}; // end if

///////////////////////////////////////////////
// main part of program stats here:


// start timed functions

// establing handler for signal 
sa.sa_flags = 0;
sa.sa_handler = funct_heartbeat;
sigemptyset(&sa.sa_mask);

ret=sigaction(SIGRTMIN, &sa, NULL);
if (ret <0) {
	fprintf(stderr,"Error in sigaction!\n");
	exit(-1);
}; // end if

/* Create the timer */
sev.sigev_notify = SIGEV_SIGNAL;
sev.sigev_signo = SIGRTMIN;
sev.sigev_value.sival_ptr = &timerid;

ret=timer_create(CLOCKID, &sev, &timerid);
if (ret < 0) {
	fprintf(stderr,"Error in timer_create!\n");
	exit(-1);
}; // end if

// start timed function, every second
its.it_value.tv_sec = 0;
its.it_value.tv_nsec = 1; // immediatly
its.it_interval.tv_sec = 1; // 1 second
its.it_interval.tv_nsec = 0;

ret=timer_settime(timerid, 0, &its, NULL);
if (ret < 0) {
	fprintf(stderr,"Error in timer_settime!\n");
	exit(-1);
}; // end if


// open inbound socket
sock_in=open_and_join_mc(s_ipaddress,s_port,verboselevel);
if (sock_in <= 0) {
	fprintf(stderr,"Error during open_and_join_mc!\n");
	exit(-1);
}; // end if

// open outbound socket
sock_out=open_for_multicast_out(verboselevel);
if (sock_out <= 0) {
	fprintf(stderr,"Error during open_for_multicast_out!\n");
	exit(-1);
}; // end if 


// convert my own ip-address into binary format
ret=inet_pton(AF_INET,myipaddress,(void *)&myaddr_bin);

// fill in fixed parts of outbound socket
MulticastOutAddr.sin6_family=AF_INET6;
MulticastOutAddr.sin6_scope_id=1;

ret=inet_pton(AF_INET6,d_ipaddress,(void *)&MulticastOutAddr.sin6_addr);
if (ret != 1) {
	fprintf(stderr,"Error: could not convert %s into valid address. Exiting\n",d_ipaddress);
	exit(1);
}; // end if


// init outbuffer
// set pointer dstkheader to beginning of buffer
dstkhead_out = (dstkheader_str *) sendbuffer;
dstkdata = (void *) sendbuffer + sizeof(dstkheader_str);

// fill in fixed parts 
dstkhead_out->version=1;

dvstreamid=0;
packetsequence=0;

while (forever) {
	// some local vars
	int dstkheaderoffset;
	int thisorigin;


	// Start receiving data from multicast stream
	// The payload of the ipv6 multicast-stream are the ipv4
	// UDP packets exchanged by the gateway-server and the
	// repeater controller	
	packetsize=recvfrom(sock_in,receivebuffer,ETHERNETMTU,0,NULL,0);

	if (packetsize == -1) {
		// no data received. Wait 2 ms and try again
		usleep(2000);
		continue;
	}; // end if

	// We should have received at least 20 octets (the size of the DSTK-header)
	if (packetsize < 20) {
		fprintf(stderr,"Packetsize to small! \n");
		continue;
	}; // end if


	dstkheaderoffset=dssextract((void *)receivebuffer,packetsize, TYPE_RPC, TYPEMASK_FILT_NOFLAGS, (void *) other, &othersize);

	// not found: print at verboselevel 2 or higher
	if (dstkheaderoffset == DSSEXTRACT_NOTFOUND) {
		if (verboselevel >= 2) {
			fprintf(stderr,"Packet found without RPC subframe\n");
		}; // end if
		continue;
	}; // end if

	// other errors: print at verboselevel 1 or higher
	if (dstkheaderoffset < 0) {
		if (verboselevel >= 1) {
			fprintf(stderr,"RPC Subframe extract error: %s\n",dssextract_printerr(dstkheaderoffset));
		}; // end if
		continue;
	}; // end if


	// dstkheader IN points to correct frame
	dstkhead_in=(dstkheader_str *)receivebuffer+dstkheaderoffset;

	// copy streamid
	streamid_instream=dstkhead_in->streamid1; // no need to convert
	// byte-order as we will just pass it on the outgoing stream

	// copy origin
	thisorigin=dstkhead_in->origin; // no need to convert as we just copy it to
	// from the incoming to the outgoing packets

	// sanity-check: check if the packet that has been received is large enough to
	// contain all data (error-check on overloading limits)
	// we are currently at the beginning of the DSTK header, the frame should be at least large
	// enough to hold the DSTK header + the IP header + UDP header
	if (packetsize < dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct udphdr) + sizeof(struct iphdr) ) {
		if (verboselevel >= 1) {
			fprintf(stderr,"Warning: received frame is not large enough to contain DSTK_HEADER + IP header + UDP header\n");
		}; // end if
		continue;
	}; // end if


	// we should have received an ipv4 IP-packet with UDP
	iphead = (struct iphdr *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str));

	// is it UDP?
	if (iphead->protocol != IPPROTO_UDP) {
		// not UDP
		if (verboselevel >= 1) {
			fprintf(stderr,"Warning: received packet is not a UDP packet!\n");
		}; // end if
		continue;
	}; // end if

	// Determine direction
	// if it send by me?
	ret=bcmp(&iphead->saddr, &myaddr_bin, sizeof(struct in_addr));

	if (ret == 0) {
		direction=1; // outbound
#if DEBUG > 1
fprintf(stderr,"OUTBOUND! \n");
#endif

	} else {
		// was it addressed to me?
		ret=bcmp(&iphead->daddr, &myaddr_bin, sizeof(struct in_addr));

		if (ret == 0) {
#if DEBUG > 1
fprintf(stderr,"INBOUND! \n");
#endif
			direction=0; // inbound
		} else {
			// not send by me or addressed to me. Ignore it
			if (verboselevel >= 1) {
				fprintf(stderr,"Error: received packet is not send by or addressed to me!\n");
			}; // end if
			continue;
		}; // end else - if
	}; // end else - if

	// go to UDP header, fetch length
	udphead = (struct udphdr *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct iphdr));

	udp_packetsize=ntohs(udphead->len) - 8; // note: udp length includes 8 bytes of the UDP header

	// sanity-check: now we know the size of the UDP-packet, we can verify if the 
	// packet that has been received is large enough to contain all data
	// (error-check on overloading limits)
	// we are currently at the beginning of the DSTK header, the frame should be at least large
	// enough to hold the DATH header + the IP header + UDP header + "udp_packet-size" of data
	if (packetsize < dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct udphdr) + sizeof(struct iphdr) + udp_packetsize ) {
		if (verboselevel >= 1) {
			fprintf(stderr,"Warning: received frame is not large enough to contain DSTK_HEADER + IP header + UDP header\n");
		}; // end if
		continue;
	}; // end if

	// go to dstar header
	dstar_rpc_head = (struct dstar_rpc_header *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct iphdr) + sizeof(struct udphdr) );


	// the first 4 octets of the header contain "DSTR"
	ret = strncmp("DSTR", (char *) dstar_rpc_head, 4);

	if (ret != 0) {
		// signature not found: not a star header
		if (verboselevel >= 1) {
			fprintf(stderr,"Error: received packet does not contain DSTAR signature\n");
		}; // end if
		continue;
	}; // end if

	// dive further into package
	dstar_data_len = ntohs(dstar_rpc_head->dstar_data_len);
#if DEBUG > 0
	fprintf(stderr,"dstar_data_len = %d \n",dstar_data_len);
#endif

	dstardata=(unsigned char *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct dstar_rpc_header) );




	// dstar_data_len can have the following values:
	// 48: first frame of DV stream
	// 19: normal frame of DV stream
	// 22: extended frame of DV stream (= normal frame + 3 octets at the end)

	// The data-transfer between the gateway-server and repeater-controller
	// happens in two phases:
	// the packet is send by one side. (rs-flag is set to 0x73)
	// after that, the reception of this packet is acknowleged by the remote
	// side (rs-flag is set to 0x72)

	// This program assumes that the exchange between the gateway-server and
	// works OK, so will just take a single-phase approach and ignore the
	// ACK messages 

	if (dstar_rpc_head->dstar_rs_flag == 0x72) {
		if (verboselevel >= 3) {
			fprintf(stderr,"RStype ACK! \n");
		}; // end if
		continue;
	}; // end if

	if (dstar_rpc_head->dstar_rs_flag != 0x73) {
		if (verboselevel >= 2) {
			fprintf(stderr,"NOT TYPE 0x73! \n");
		}; // end if
		continue;
	}; // end if


	if (dstar_rpc_head->dstar_pkt_type != DSTAR_PKT_TYPE_DV) {
		// not a packet used for DV: ignore it
		if (verboselevel >= 2) {
			//fprintf(stderr,"Error: not a DV type packet: %X \n",dstar_rpc_head->dstar_pkt_type);
			fprintf(stderr,"NDV ");
		}; // end if
		continue;
	}; // end if


	// Check size
	if ((dstar_data_len != 19) && (dstar_data_len != 22) && (dstar_data_len != 48)) {
		if (verboselevel >= 1) {
			fprintf(stderr,"Error: Received DV packet has incorrect length: %d\n",dstar_data_len);
		}; // end if
		continue;
	}; // end if


	// set pointers to dv_header
	dv_header=(struct dstar_header2 *) dstardata;

	// info: status: 0 = no DV stream active, 1 = stream active

#if DEBUG > 1
fprintf(stderr,"dstar_data_len = %d \n",dstar_data_len);
#endif



	// what kind of packet it is?
	// is it a addressing/routing frame
	if (dstar_data_len == 48) {
		// set pointer for rf_header
		// sizeof(struct dstar_header2 = 7 octets, but due to 16 bit boundary, "sizeof" returns 8
		dv_artheader=(struct dstar_dv_artheader *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct dstar_rpc_header) + 7 );

		// determine module
		if (direction) {
			// outbound -> module is last character of rpt 2
			thismodule_c=dv_artheader->rpt2_callsign[7];
		} else {
			// inbound -> compair with last character of rpt 1
			thismodule_c=dv_artheader->rpt1_callsign[7];
		}; // end if


		// print addressing/routing information
		if (verboselevel >= 2) {
			char t[9];
			t[8]=0;

			memcpy(t,dv_artheader->rpt2_callsign,8);
			fprintf(stderr,"\nRPT2:%s",t);

			memcpy(t,dv_artheader->rpt1_callsign,8);
			fprintf(stderr,",RPT1:%s",t);

			memcpy(t,dv_artheader->your_callsign,8);
			fprintf(stderr,",YR:%s",t);

			memcpy(t,dv_artheader->my_callsign,8);
			fprintf(stderr,",MY:%s",t);

			memcpy(t,dv_artheader->my_callsign_ext,4);
			t[4]=0; t[5]=0; t[6]=0; t[7]=0;
			fprintf(stderr,",MYEXT:%s\n",t);
		}

		if ((thismodule_c == 'A' || thismodule_c == 'a')) {
			thismodule_i=0; thismodule_c='A';
		} else if ((thismodule_c == 'B' || thismodule_c == 'b')) {
			thismodule_i=1; thismodule_c='B';
		} else if ((thismodule_c == 'C' || thismodule_c == 'c')) {
			thismodule_i=2; thismodule_c='C';
		} else {
			fprintf(stderr,"Warning, received routing/addressing packet does not have valid module-name: %c\n",thismodule_c);
			continue;
		}; // end if

		// if is a module we are interested in?
		if (! (modactive[thismodule_i])) {
			if (verboselevel >= 2) {
				fprintf(stderr,"Message, received packet for module we are not interested in: %d\n",thismodule_i);
			}; // end if
			continue;
		}; // end if

		// accept it when
		// status is 0 (no DV-stream active)
		// or if timeout (additional check to deal with stale sessions where we
		// did not receive an "end-of-stream" packet)

		// the "inbound timeout" value is "armed" with 50 in this function and decreased by
		// one in the "serialsend" function. As that function is started every 20 ms, the
		// session will timeout after 1 second.

		if ((activestatus[thismodule_i] == 0) || (global.inbound_timeout[thismodule_i] == 0)) {

			// copy streamid, set frame counter, set status and go to next packet
			if (verboselevel >= 1) {
				fprintf(stderr,"NS%X/%04X \n",direction,dv_header->streamid);
			}; // end if

			activestreamid[thismodule_i]=dv_header->streamid;
			activedirection[thismodule_i]=direction;
			activestatus[thismodule_i]=1;

		} else {
			if (verboselevel >= 2) {
				//fprintf(stderr,"Error: DV header received when DV voice-frame expected\n");
				fprintf(stderr,"DVH ");
			}; // end if
			continue;
		}; // end else
	
		// frame is OK, fill in packet-type in header outgoing packet, including flags
		if (direction) {
			dstkhead_out->type=htonl(TYPE_AMB_ART | TYPEMASK_FLG_DIR);
		} else {
			dstkhead_out->type=htonl(TYPE_AMB_ART);
		}; // end else - if



	} else {
		// DV-data received

		// set pointer for rf_data
		// sizeof(struct dstar_header2 = 7 octets, but due to 16 bit frontier, "sizeof" returns 8
		dv_data=(struct dstar_dv_data *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct dstar_rpc_header) + 7 );

		thismodule_i=-1;
		// determine module, based on streamid
		for (loop=0;((loop<=2) && (thismodule_i==-1));loop++) {
			if ((modactive[loop]) && (dv_header->streamid == activestreamid[loop])) {
				thismodule_i=loop;
				thismodule_c=(char) (0x41+loop);
			}; // end if
		}; // end for
			
		if (thismodule_i == -1) {
			if (verboselevel >= 2) {
//				fprintf(stderr,"Warning: received stream not linked to any active streams: %04X\n",dv_header->streamid);
				fprintf(stderr,"NAS%X/%04X ",direction,dv_header->streamid);
			}; // end if
			continue;
		};


		// is it the correct direction?
		if (direction != activedirection[thismodule_i]) {
			if (verboselevel >= 1) {
				fprintf(stderr,"Warning: Receiving stream in opposite direction for stream %04X on module %d\n",dv_header->streamid,thismodule_i);
			}; // end if
			continue;
		}; // end if


		// check sequence number: should be between 0 and 20
		this_sequence=(uint8_t) dv_header->dv_seqnr & 0x1f;

		// error check
		if (this_sequence > 20) {
			if (verboselevel >= 1) {
				fprintf(stderr,"Error: Invalid sequence number received: %d\n",this_sequence);
			}; // end if
			continue;
		}; // end if

		// last frame of stream? (6th bit set?)
		if ((uint8_t) dv_header->dv_seqnr & 0x40) {
			// set status to "off"
			activestatus[thismodule_i]=0;
fprintf(stderr,"ES%04X \n",dv_header->streamid);
		}; // end if

		// frame is OK, fill in packet-type in header outgoing packet
		// also set "direction" and "RPT1" flags
		if (dstar_data_len == 19) {
			// standard digital voice frame
			if (direction) {
				dstkhead_out->type=htonl(TYPE_AMB_DV | TYPEMASK_FLG_DIR);
			} else {
				dstkhead_out->type=htonl(TYPE_AMB_DV);
			}; // end else - if
		} else {
			// extended digital voice frame (length 22)
			if (direction) {
				dstkhead_out->type=htonl(TYPE_AMB_DVE | TYPEMASK_FLG_DIR);
			} else {
				dstkhead_out->type=htonl(TYPE_AMB_DVE);
			}; // end else - if
		}; // end else - if


	}; // end else - if

	
	// OK, if the packet has not been rejected by all these checkes, re-broadcast it
	// fill in rest of header
	dstkhead_out->sequence.seq32.seq1=htons(packetsequence);
	dstkhead_out->sequence.seq32.seq2=0;

	packetsequence++;
	dstkhead_out->streamid1=htons((uint32_t)dv_header->streamid);
	// streamid2 is copy of streamid1 of incoming 
	dstkhead_out->streamid2=streamid_instream;

	// origin
	dstkhead_out->origin=thisorigin;

	// copy data to dstkdata
	memcpy(dstkdata,dstardata,dstar_data_len);

	// set size
	dstkhead_out->size=htons(dstar_data_len);


	// is there any "other" data?
	if (othersize == 0) {
		// set "last" flag.
		dstkhead_out->flags |= DSTK_FLG_LAST;
	} else {
		// There is "other" data
		// check if we have sufficient place in outbuffer
		if (sizeof(dstkheader_str) + dstar_data_len + othersize > ETHERNETMTU) {
			if (verboselevel >= 1) {
				fprintf(stderr,"Warning, output buffer not large enough to store \"other\" data\n");
			}; // end if

			// continue as there was no "other" data
			othersize=0;

			// set "last" flag.
			dstkhead_out->flags |= DSTK_FLG_LAST;
		} else {
			// clear "last" flag (just to be sure)
			dstkhead_out->flags &= (0xFF ^ DSTK_FLG_LAST);

			// copy rest of "other"
			memcpy(dstkhead_out+sizeof(dstkheader_str)+dstar_data_len,other,othersize);
		}; // end else - if
		

	}; // end else - if


	// set port
	MulticastOutAddr.sin6_port=htons((unsigned short int) d_port + thismodule_i * d_portincr);

	// copy data
	ret=sendto(sock_out,dstkhead_out,sizeof(dstkheader_str) + dstar_data_len,0,(struct sockaddr *) &MulticastOutAddr, sizeof(struct sockaddr_in6));

	if (verboselevel >= 1) {
		fprintf(stderr,"%c",thismodule_c);
	}; // end if

	if (ret < 0) {
		if (verboselevel >= 1) {
			fprintf(stderr,"Warning: sendto fails (error = %d(%s))\n",errno,strerror(errno));
		}; // end if
	}; // end if
	global.inbound_timeout[thismodule_i]=3;

}; // end while (forever)


// outside "forever" loop: we should never get here unless something went wrong



return(0);

};


// heartbeat function
// started every second, decreases 
void funct_heartbeat (int sig) {
int loop;

for (loop=0; loop<=2; loop++) {
	// decrease timeout counter if higher then 0

	if (global.inbound_timeout[loop]) {
		global.inbound_timeout[loop]--;

		// print DEBUG line when timer reaches 0
		if (! global.inbound_timeout[loop]) {
			fprintf(stderr,"TS%d \n",loop);
		}; // end if
	}; // end if

}; // end for
}; // end function funct_heartbeat;
