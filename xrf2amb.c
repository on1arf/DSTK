/* xrf2amb.c */

// xrf2amb: link to a dextra X-reflector and re-broadcast the received
// AMBE-streams as a DSTK AMBE stream

// Usage: xrf2amb [-v] [ [-4] || [-6] ]  [-di ipaddress] [-dp port] [-dpi portincrease] [-rp reflectorport] MYCALL reflectorhost
// Usage: xrf2amb -h
// Usage: xrf2amb -V

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

#define VERSION "0.0.1"
#define XRF2AMB 1

// change this to 1 for debugging
#define DEBUG 0


#include <stdio.h>
#include <stdlib.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <string.h>

#include <unistd.h>

// posix threads
#include "pthread.h"

// needed for gettimeofdat (used with srandom)
#include <sys/time.h>

// posix threads + inter-process control
#include <signal.h>


// global structured and defines for DSTAR Audio TookKit
#include "dstk.h"

// structures and data specific for cap2rpc
#include "xrf2amb.h"

// functions for multicasting
#include "multicast.h"

// dextra linking and heartbeat functions
#include "dextra.h"



//////////////////////////////////////////////////////////////////////////
// configuration part // // configuration part // // configuration part //
//////////////////////////////////////////////////////////////////////////

char * default_d_ipaddress="ff11::4200";
int default_d_port=4200;
int default_d_portincrease=1;

int default_ipv4only=0;
int default_ipv6only=0;

int default_reflectorport=30001;

// end configuration part. Do not change below unless you
// know what you are doing

// usage and help
// should be included AFTER setting up the default values
#include "helpandusage.h"


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Main program // Main program // Main program // Main program //////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main (int argc, char * argv[]) {

unsigned char sendbuffer[ETHERNETMTU];
unsigned char receivebuffer[ETHERNETMTU];

dstkheader_str * dstkheader;
void * dstkdata;

char * mycall=NULL;
char * reflectorhost=NULL;



// outgoing udp stream
int sock_out;
struct sockaddr_in6 MulticastOutAddr;
char * d_ipaddress;
int d_port; int d_portincrease;

// posix threads
pthread_t thr_dextraheartbeat;

// headers
int packetsequence;


// some other vars
int ret;
int found;
int timeouts;
int calllen;
int streamloop;
int paramloop;
char module_c; int module_i;

// time structures: used with srandom
struct timeval now;
struct timezone tz;

// vars for getaddrinfo
struct addrinfo hint;
struct addrinfo * info;

int ipv4only;
int ipv6only;

// information about modules and streams
int modactive[3];

// as a reflector can have multiple streams active at the
// same time, even multiple streams on the same module, we just cache up to
// MAXSTREAMACTIVE active streams
// Even streams on modules we are not subscribed are cached to avoid wrong errormessages
int stream_streamid[MAXSTREAMACTIVE];
char stream_module[MAXSTREAMACTIVE];

// stream_active and stream_timeout have been moved to global memory
//int stream_active[MAXSTREAMACTIVE];	// 0: memory position not in use, 1: in use on a module to which
													// we are subscribed, -1: in use on a module to which we are
													// NOT subscribed
//int stream_timeout[MAXSTREAMACTIVE];

// vars for select and to receive data
fd_set rfds;
struct timeval tv;
int recvlen;

// pointers to process received data
struct dstar_str_header1 * this_dsstrhead1;
struct dstar_str_header2 * this_dsstrhead2;
struct dstar_dv_rf_header * this_dv_rfhead;
struct dstar_dv_data * this_dv_data;
int this_streamid;
uint32_t this_type;

// ////// data definition done ///


// main program starts here
// init global vars
global.outsock=0;
global.verboselevel=0;

global.destport=default_reflectorport;

// init other vars
modactive[0]=1; modactive[1]=1; modactive[2]=1;
ipv4only=default_ipv4only; ipv6only=default_ipv6only;
d_ipaddress=default_d_ipaddress; d_port=default_d_port; d_portincrease=default_d_portincrease;



// cli option parsing
// Usage: xrf2amb [-v] [ [-4] || [-6] ]  [-di ipaddress] [-dp port] [-dpi portincrease] [-rp reflectorport] MYCALL reflectorhost

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
		global.verboselevel++;
	} else if (strcmp(thisarg,"-di") == 0) {
		// -si = DESTINATION ipaddress
		if (paramloop+1 < argc) {
			paramloop++;
			d_ipaddress=argv[paramloop];
		}; // end if
	} else if (strcmp(thisarg,"-dp") == 0) {
		// -sp = DESTINATION port
		if (paramloop+1 < argc) {
			paramloop++;
			d_port=atoi(argv[paramloop]);
		}; // end if
	} else if (strcmp(thisarg,"-dpi") == 0) {
		// -sp = DESTINATION port increase
		if (paramloop+1 < argc) {
			paramloop++;
			d_portincrease=atoi(argv[paramloop]);
		}; // end if
	} else if (strcmp(thisarg,"-4") == 0) {
		// -4 = ipv4only
		ipv4only=1;
	} else if (strcmp(thisarg,"-6") == 0) {
		// -6 = ipv6only
		ipv6only=1;
	} else if (strcmp(thisarg,"-rp") == 0) {
		// -rp = remote REFLECTOR port
		if (paramloop+1 < argc) {
			paramloop++;
			global.destport=atoi(argv[paramloop]);
		}; // end if
	} else {
		// last parameters, first the MYCALL, then reflectorhostname
		if (!(mycall)) {
			mycall=argv[paramloop];
		} else if (!(reflectorhost)) {
			reflectorhost=argv[paramloop];
		} else {
			fprintf(stderr,"Warning: Argument to much: %s\n",argv[paramloop]);
		}; // end if
	}; // end - elsif - if

}; // end for


// sanity checks

// sufficient parameters
// we should have at least 2 parameters: mycall and reflectorhost
if ((!(mycall)) || (!(reflectorhost))) {
	fprintf(stderr,"Error: Unsufficient parameters! \n");
	usage(argv[0]);
	exit(-1);
}; // end if


calllen=strlen(mycall);
if (calllen > 6) {
	calllen=6;
}; // end if

if (calllen < 3) {
	fprintf(stderr,"Error: Call should be at least 3 characters! \n");
	usage(argv[0]);
	exit(-1);
}; // end if

// check there is no "/" in the callsign
if ((memchr(mycall,'/',6) != NULL)) {
	fprintf(stderr,"Error: mycall should not contain a '/'\n");
	usage(argv[0]);
	exit(-1);
}; // end if

// clean mycall
memset(global.mycall,' ',6);
memcpy(global.mycall,mycall,calllen);


// sanity checks: UDP port should be between 1 and 65535
if ((global.destport<1)  || (global.destport > 65535)) {
	fprintf(stderr,"Error: remote reflector UDP port should be between 1 and 65535\n");
	usage(argv[0]);
	exit(-1);
}; // end if

if ((d_port<1)  || (d_port > 65535)) {
	fprintf(stderr,"Error: source UDP  port should be between 1 and 65535\n");
	usage(argv[0]);
	exit(-1);
}; // end if


// sanity check: ipv4only and ipv6only
if ((ipv4only) && (ipv6only)) {
	fprintf(stderr,"Error: ipv4only and ipv6only are mutually exclusive\n");
	usage(argv[0]);
	exit(-1);
}; // end if




// init vars and buffers
for (streamloop=0; streamloop<MAXSTREAMACTIVE; streamloop++) {
	global.stream_active[streamloop]=0;
}; // end for

dstkheader = (dstkheader_str *) sendbuffer;
dstkdata = (void *) sendbuffer + sizeof(dstkheader_str);

// reset all values to 0
memset(dstkheader,0,sizeof(dstkheader_str));

// fill in fixed parts
dstkheader->version=1;
// only one single subframe in a DTAK frame: set next-header to 0
dstkheader->flags=0 | DSTK_FLG_LAST;

dstkheader->origin=htons(ORIGIN_CNF_DEX);

// streamid is random
gettimeofday(&now,&tz);
srandom(now.tv_usec);

dstkheader->streamid1=random();
// streamid2 is copy of streamid of AMBE-stream
// copied further down

// sequence 2 is not used. Set to 0
dstkheader->seq2=0;


// /// open socket for outgoing UDP multicast
sock_out=open_for_multicast_out(global.verboselevel);

if (sock_out <= 0) {
	fprintf(stderr,"Error during open_for_multicast_out! Exiting!\n");
	exit(-1);
}; // end if

global.outsock=sock_out;

// packets are send to multicast address, set UDP port
MulticastOutAddr.sin6_family = AF_INET6; 
MulticastOutAddr.sin6_scope_id = 1;
// do not fill in port yet


ret=inet_pton(AF_INET6,d_ipaddress,(void *)&MulticastOutAddr.sin6_addr);
if (ret != 1) {
	fprintf(stderr,"Error: could not convert %s into valid address. Exiting\n",d_ipaddress);
	exit(1);
}; // end if

// dextra related stuff

// DNS lookup for remote Dextra repeater/reflector

// clear hint
memset(&hint,0,sizeof(hint));

hint.ai_socktype = SOCK_DGRAM;

// resolve hostname, use function "getaddrinfo"
// set address family in hint if ipv4only or ipv6ony
if (ipv4only) {
	hint.ai_family = AF_INET;
} else if (ipv6only) {
	hint.ai_family = AF_INET6;
} else {
	hint.ai_family = AF_UNSPEC;
}; // end if

// do DNS-query, use getaddrinfo for both ipv4 and ipv6 support
ret=getaddrinfo(reflectorhost, NULL, &hint, &info);

if (ret != 0) {
	fprintf(stderr,"Error: resolving hostname: (%s)\n",gai_strerror(ret));
	exit(-1);
}; // end if

// getaddrinfo can return multiple results, we only use the first one
// give warning is more then one result found.
// Data is returned in info as a linked list
// If the "next" pointer is not NULL, there is more then one
// element in the chain

if ((info->ai_next != NULL) || global.verboselevel >= 1) {
	char ipaddrtxt[INET6_ADDRSTRLEN];

	// get ip-address in numeric form
	if (info->ai_family == AF_INET) {
		// ipv4
		// for some reason, we neem to shift the address-information with 2 positions
		// to get the correct string returned
		inet_ntop(AF_INET,&info->ai_addr->sa_data[2],ipaddrtxt,INET6_ADDRSTRLEN);
	} else {
		// ipv6
		// for some reason, we neem to shift the address-information with 6 positions
		// to get the correct string returned
		inet_ntop(AF_INET6,&info->ai_addr->sa_data[6],ipaddrtxt,INET6_ADDRSTRLEN);
	}; // end else - if

	if (info->ai_next != NULL) {
		fprintf(stderr,"Warning. getaddrinfo returned multiple entries. Using %s\n",ipaddrtxt);
	} else {
		fprintf(stderr,"Connecting to dextra gateway %s\n",ipaddrtxt);
	}; // end if
}; // end if

// store address info in global structure
global.ai_addr=(struct sockaddr_in6 *) info->ai_addr;


// start dextra session
ret=dextralink(sock_out, mycall, calllen, (struct sockaddr_in6 *) info->ai_addr, global.destport);


// starts thread that sends a heartbeat every 6 seconds
pthread_create(&thr_dextraheartbeat,NULL,funct_dextraheartbeat, (void *) &global);



// endless loop
timeouts=0;
packetsequence=0;

while (forever) {


	// blocking read with timeout: so we use a select
	// set timeout to 1 second
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	FD_ZERO(&rfds);
	FD_SET(sock_out, &rfds);

	ret=select(sock_out+1,&rfds,NULL,NULL,&tv); 

	if (ret < 0) {
		fprintf(stderr,"Error: select fails (errno=%d)\n",errno);
		exit(-1);
	}; // end if

	// ret=0, we hit a timeout
	if (ret == 0) {
		timeouts++;

		// we stop if we have not received any data during 1 minute
		// as we are supposted to receive heartbeats every 6 seconds
		// from the remote side, this is a very long periode
		if (timeouts > 60) {
			break;
		}; // end if

		continue;
	}; // end if

	// reset timeouts
	timeouts=0;

	// read the packet.
	// as there was only one single socket in the "select", we do not need to ask which socket has data

	recvlen=recv(sock_out,receivebuffer,ETHERNETMTU,MSG_DONTWAIT);


	if (recvlen < 0) {
		// no data to be received.
		// this should not happen as the "select" told us there is data waiting for us
	}; // end if

	// we have received a frame. Check size.
	// len=9 = incoming heartbeat: ignore
	// len=29 or 32: DV frame
	// len=54: DV "CFG" frame

	if (recvlen == 9) {
		// heartheat: ignore
if (global.verboselevel >= 3) {
	fprintf(stderr,"H ");
}; // end if
		continue;
	} else if (recvlen == 27) {
		this_type=TYPE_AMB_DV;
	} else if (recvlen == 30) {
		this_type=TYPE_AMB_DV;
	} else if (recvlen == 56) {
		this_type=TYPE_AMB_CFG;
	} else {
		if (global.verboselevel >= 2) {
			fprintf(stderr,"Error: packet has invalid size: %d\n",recvlen);
		}; // end if
		continue;
	}; // end else - elsif ... - if
		
		
	this_dsstrhead1 = (struct dstar_str_header1 *) (receivebuffer);
	this_dsstrhead2 = (struct dstar_str_header2 *) (receivebuffer + sizeof(struct dstar_str_header1));


	// First 4 octets should be 'DSTR'
	if (strncmp("DSVT", this_dsstrhead1->dstarstr_id,4) != 0) {
		// not a DSTAR packet
		if (global.verboselevel >= 1) {
			fprintf(stderr,"Error: DSTV fingerprint not found\n");
			continue;
		}; // end if
	}; // end if

	// voice/data should be set to voice
	if (this_dsstrhead2->dstarstr_voicedata != DSSTR_VOICEDATA_VOICE) {
		// not a DV packet
		if (global.verboselevel >= 1) {
			fprintf(stderr,"Error: packet is not a voice packet\n");
			continue;
		}; // end if
	}; // end if

	// get streamid;
	this_streamid = this_dsstrhead2->dstarstr_streamid;

	// config-frame (first frame of stream);
	if (recvlen == 56) {
		int modulesubscribed;

		this_dv_rfhead=(struct dstar_dv_rf_header *) (receivebuffer + sizeof(struct dstar_str_header1) + sizeof(struct dstar_str_header2));

		// module is last character of RPT2 field
		module_c=this_dv_rfhead->rpt2_callsign[7];

		// sanity check
		module_i=-1;
		if ((module_c == 'a') || (module_c == 'A')) {
			module_i=0; module_c='A';
		} else if ((module_c == 'b') || (module_c == 'B')) {
			module_i=1; module_c='B';
		} else if ((module_c == 'c') || (module_c == 'C')) {
			module_i=2; module_c='C';
		}; // end elsif - elsif - if

		// valid module?
		if  (module_i == -1) {
			if (global.verboselevel >= 1) {
				fprintf(stderr,"Error: Invalid module: %c\n",module_c);
				continue;
			}; // end if
		}; // end if

		if (global.verboselevel >= 2) {
			fprintf(stderr,"NS%c%04X ",module_c,this_streamid);
		}; // end if

		if (! (modactive[module_i])) {
			if (global.verboselevel >= 1) {
				fprintf(stderr,"Error: Stream for module we are not interested in: module:%c streamid:%04X\n",module_c,this_streamid);
			}; // end if
			modulesubscribed=-1;
		} else {
			modulesubscribed=1;
		}; // end else - if

		// remember streamid
		// first check if this is not a repetition of an earlier
		// "start-stream" message
		found=0;
		for (streamloop=0; streamloop < MAXSTREAMACTIVE; streamloop++) {
			if ((global.stream_active[streamloop]) && stream_streamid[streamloop] == this_streamid) {
				found=1;
				break;
			}; // end if
		}; // end 
		
		if (found) {
			if (global.verboselevel >= 2) {
//				fprintf(stderr,"Warning: stream already known: module:%c streamid:%04X\n",module_c,this_streamid);
				fprintf(stderr,"AK%c%04X ",module_c,this_streamid);
			}; // end if
		} else {
			// find a place to start this streamid
			found=0;
			for (streamloop=0; ((streamloop < MAXSTREAMACTIVE) && (!(found))); streamloop++) {
				if (!(global.stream_active[streamloop])) {
					global.stream_active[streamloop]=modulesubscribed;
					stream_streamid[streamloop]=this_streamid;
					global.stream_timeout[streamloop]=10;
					found=1;

					// store module. Not necessairy, but nice to print out for debuging
					stream_module[streamloop]=module_c;
				}; // end if
			}; // end for

			if (!(found)) {
				// oeps. No place to store this stream. All streams in cache
				if (global.verboselevel >= 1) {
					fprintf(stderr,"Warning: stream could not be stored! module:%c streamid:%04X\n",module_c,this_streamid);
				}; // end if
			// go to next packet
			continue;
			}; // end if (found)
		}; // end else - if (store streamid)

	} else {
		// recvlen = 29 or 32: DV data frame
		int streamidsubscribed;

		this_dv_data=(struct dstar_dv_data *) (receivebuffer + sizeof(struct dstar_str_header1) + sizeof(struct dstar_str_header2));

		// search for streamid in list
		found=0; streamidsubscribed=0;
		for (streamloop=0; ((streamloop < MAXSTREAMACTIVE) && (!(found))); streamloop++) {
			if ((global.stream_active[streamloop]) && (this_streamid == stream_streamid[streamloop])) {
				found=1;
				// is if from a module on which we are subscribed?
				if (global.stream_active[streamloop] > 0) {
					streamidsubscribed=1;
				}; // end if

				// get module (for debug output)
				module_c=stream_module[streamloop];

				// is it the last packet of a stream?
				if (this_dv_data->dv_seqnr & 0x40) {
					// clear stream in cache
fprintf(stderr,"ES%04X\n",this_streamid);
					global.stream_active[streamloop]=0;
				} else {
					// rearm timeout
					global.stream_timeout[streamloop]=10; 
				}; // end else - if

			}; // end if
		}; // end for



		// if not a stream to which we are subscribed -> skip packet
		if (!(streamidsubscribed)) {
fprintf(stderr,"NOTSUB%04x ",this_streamid);
			continue;
		}; // end if

	}; // end else - if

	// set packetsequence
	dstkheader->seq1=htons(packetsequence);
	packetsequence++;

	// copy dvdata from DV-data to end, include dstar stream headers
	memcpy(dstkdata,receivebuffer,recvlen);

	// copy streamid of incoming AMBE-stream to streamid2 of DSTK-stream
	dstkheader->streamid2=this_streamid;

	// set type
	// also set "AMBE STREAM header 1" flag (see dstk.h for more info)
	this_type |= TYPEMASK_FLG_AMB_STRHDR1;
	dstkheader->type=htonl(this_type);

	// set size
	dstkheader->size=htons(recvlen);

	if (global.verboselevel >= 2) {
		fprintf(stderr,"%c",module_c);
	}; // end if

	// fill in destination port
	MulticastOutAddr.sin6_port = htons((unsigned short int) d_port + d_portincrease * module_i);

	// send DSTAR frame
	ret=sendto(sock_out,dstkheader,recvlen+sizeof(dstkheader_str),0,(struct sockaddr *) &MulticastOutAddr, sizeof(MulticastOutAddr));

	if (ret < 0) {
		// error
		fprintf(stderr,"udp packet could not be send %d (%s)!\n",errno,strerror(errno)); 
	}; // end if

}; // end while

return(0);

}; // end main
