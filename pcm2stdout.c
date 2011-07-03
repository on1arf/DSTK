/* pcm2stdout.c */

// pcm2stdout: join PCM-stream and send to standard out, optionally
// adding silence or noise when no voice is being received

// This application can be used to save a PCM-stream to a file or
// pipe it to an external application for further processing
// As the lenght of the PCM-stream is not known beforehand, this application
// generated "raw" PCM without a PCM-header

// Usage: pcm2stdout [-v] [ [-s] || [-n] ] [-si ipaddress] [-sp port] 
// Usage: pcm2stdout -h
// Usage: pcm2stdout -V

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
#define PCM2STDOUT 1

// error numbers
#include <errno.h>

// change this to 1 for debugging
#define DEBUG 0

#include <stdlib.h>
#include <stdio.h>
// needed for usleep
#include <unistd.h>
//#include <fcntl.h>
#include <string.h>
#include <stdint.h>

// sockets
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

// header-files for ip, udp and other
#include <linux/ip.h>
#include <linux/udp.h>

// timed interrupts
#include <time.h>
#include <signal.h>


// needed for gettimeofday (used with srandom)
#include <sys/time.h>

// global structures and defines of dstk
#include "dstk.h"

// cap2mc.h: data structures
#include "pcm2stdout.h"

// include functions for multicasting
#include "multicast.h"

// defines for timed interrupts
#define CLOCKID CLOCK_REALTIME
#define SIG SIGRTMIN

// define for noise generator
#define NOISEFRAMES 20
#define NOISEVOL 0x1F


//////////////////////////////////////////////////////////////////////////
// configuration part // // configuration part // // configuration part //
//////////////////////////////////////////////////////////////////////////

char * default_s_ipaddress="ff11::4300";
int default_s_port=4300;

int default_addsilence=0;
int default_addnoise=0;

// end configuration part. Do not change below unless you
// know what you are doing

// usage and help
// should be included AFTER setting up the default values
#include "helpandusage.h"



// function defined further down
static void funct_senddata ();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Main program // Main program // Main program // Main program //////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


int main(int argc,char** argv) {

// data structures:

// some miscellaneous vars:
char * s_ipaddress;
int s_port;

// loops
int paramloop;

// local vars
unsigned char receivebuffer[ETHERNETMTU];

// misc vars
int packetsize;

int ret;

int foundit; int giveup; int otherfound;


// vars for timed interrupts
struct sigaction sa;
struct sigevent sev;
timer_t timerid;
struct itimerspec its;

// vars to process the streams
int dstkheaderoffset;
char * pcmdata;

// networking vars
int sock_in;

// counter so that not all output comes on one line
int dotcounter;

// main program starts here ///
// part 1: initialise vars and check cli-arguments

global.verboselevel=0;
s_ipaddress=default_s_ipaddress;
s_port=default_s_port;

global.addsilence=default_addsilence;
global.addnoise=default_addnoise;


// CLI option decoding
// Usage: pcm2stdout [-v] [-si ipaddress] [ -sp port] 

for (paramloop=1;paramloop<argc;paramloop++) {
	char * thisarg=argv[paramloop];

	if (strcmp(thisarg,"-V") == 0) {
		// -V = version
			fprintf(stderr,"%s version %s\n",argv[0],VERSION);
			exit(0);
	} else if (strcmp(thisarg,"-v") == 0) {
		// -v = verbose
		global.verboselevel++;
	} else if (strcmp(thisarg,"-si") == 0) {
		// -si = SOURCE ipaddress
		if (paramloop+1 < argc) {
			paramloop++;
			s_ipaddress=argv[paramloop];
		}; // end if
	} else if (strcmp(thisarg,"-sp") == 0) {
		// -sp = SOURCE port
		if (paramloop+1 < argc) {
			paramloop++;
			s_port=atoi(argv[paramloop]);
		}; // end if
	} else if (strcmp(thisarg,"-s") == 0) {
		// -s = addsilence
		global.addsilence=1;
	} else if (strcmp(thisarg,"-n") == 0) {
		// -n = addnoise
		global.addnoise=1;
	} else if (strcmp(thisarg,"-h") == 0) {
		// -h = help
		help(argv[0]);
		exit(-1);
	} else {
		// unknown option
		fprintf(stderr,"Error: unknown option %s\n",argv[paramloop]);
		usage(argv[0]);
		exit(-1);
	}; // end elsif - elsif - elsif - if

}; // end for


// some error checking

// addsilence and addnoise are mutually exclusive
if ((global.addsilence) && (global.addnoise)) {
	fprintf(stderr,"Error: addsilence and addnoise are mutually exclusive\n");
	usage(argv[0]);
	exit(-1);
}; // end if


// Part 2 of main program

// init global vars
// allocate memory for buffer
global.buffer[0]=(unsigned char *) malloc(320);
global.buffer[1]=(unsigned char *) malloc(320);

if ((global.buffer[0] == NULL) || (global.buffer[1] == NULL)) {
	fprintf(stderr,"Error: could not allocate memory for buffers!\n");
	exit(-1);
}; // end if

global.activebuffer=0;
global.datatosend=0;

// start timer, start the "funct_senddata" function every 20 ms
sa.sa_flags=0;
sa.sa_handler = funct_senddata;
sigemptyset(&sa.sa_mask);

ret=sigaction(SIG,&sa,NULL);
if (ret < 0) {
	fprintf(stderr,"Error in sigaction! \n");
	exit(-1);
}; // end if

// create timer
sev.sigev_notify = SIGEV_SIGNAL;
sev.sigev_signo = SIG;
sev.sigev_value.sival_ptr = &timerid;

ret=timer_create(CLOCKID,&sev,&timerid);
if (ret < 0) {
	fprintf(stderr,"Error in timer_create! \n");
	exit(-1);
}; // end if

// start timed function, every 20 ms
its.it_value.tv_sec = 0;
its.it_value.tv_nsec = 1; // start immediatly
its.it_interval.tv_sec = 0;
its.it_interval.tv_nsec = 20000000; // 20 ms = 20 million nanoseconds

ret=timer_settime(timerid, 0, &its, NULL);
if (ret < 0) {
	fprintf(stderr,"error in timer_settime!\n");
	exit(-1);
}; // end if


// init networking stuff
sock_in=open_and_join_mc(s_ipaddress,s_port, global.verboselevel);

if (sock_in < 0) {
	fprintf(stderr,"open and join multicast failed! \n");
}; // end if


// make stdout unbuffered
setvbuf(stdout,NULL,_IONBF,0);


dotcounter=0;
while (forever) {
	dstkheader_str * dstkhead;
        
	packetsize=recvfrom(sock_in,receivebuffer,ETHERNETMTU,0,NULL,0);

#if DEBUG > 1
fprintf(stderr,"\n*** NEW  ... packetsize = %d \n",packetsize);
#endif


	if (packetsize == -1) {
		// nothing received
		usleep(10000);
		continue;
	}; // end if

	if (packetsize <= 0) {
		fprintf(stderr,"Packetsize to small! \n");
		continue;
	}; // end if

	// check packet: we should find a DSTK frame
	foundit=0; giveup=0; dstkheaderoffset=0; otherfound=0;
	while ((! foundit) && (! giveup)) {
		// check DSTK packet header
		dstkhead = (dstkheader_str *) (receivebuffer + dstkheaderoffset);

		if (dstkhead->version != 1) {
			fprintf(stderr,"DSTK header version 1 expected. Got %d\n",dstkhead->version);
			giveup=1;
			break;
		} else if ((ntohl(dstkhead->type) & TYPEMASK_FILT_GRP) == TYPE_PCM) {
			// OK, found a DSTK subframe of the group TYPE_PCM
			foundit=1;
			break;
		}; // end if

		// OK, we found something, but it's not what we are looking for
		otherfound=1;

		if ( (! (dstkhead->flags | DSTK_FLG_LAST)) &&
			(dstkheaderoffset+ntohs(dstkhead->size)+sizeof(dstkheader_str) +sizeof(dstk_signature) <= packetsize )) {
			// not yet found, but there is a pointer to a structure further
			// on in the received packet
			// And it is still within the limits of the received packet

			// move up pointer
			dstkheaderoffset+=sizeof(dstkheader_str)+ntohs(dstkhead->size);

			// check for signature 'DSTK'
			if (memcmp(&receivebuffer[dstkheaderoffset],dstk_signature,sizeof(dstk_signature))) {
				// signature not found: give up
				giveup=1;
			} else {
				// signature found: move up pointer by 4 octets and repeat while-loop
				dstkheaderoffset += sizeof(dstk_signature);
			}; // end if                            
		} else {
			// give up
			giveup=1;
		}; // end else - elsif - elsif - if

	}; // end while

	if (giveup) {
		if (global.verboselevel >= 1) {
			if (otherfound) {
				fprintf(stderr,"Warning: received packet does not contain RPCIP sub-packet!\n");
			} else {
				fprintf(stderr,"Warning: received packet is not a DSTK packet!\n");
			}; // end else - if
		}; // end if
		continue;
	}; // end if


	// sanity check: checck size: we should now have at least 320 octets
	if (packetsize < dstkheaderoffset + sizeof(dstkheader_str) + 320) {
		if (global.verboselevel >= 1) {
			fprintf(stderr,"Warning, received packet not large enough to contain DSTK_HEADER + 320 octets of PCM information!\n");
		}; // end if
		continue;
	}; // end if


	pcmdata = (char  *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str));

	// if not a filler packet
	if (ntohl(dstkhead->type) != TYPE_PCM_FLL) {
		int newbuffer;

		// mutex code needed here

		// some error handling
		if ((global.activebuffer == 0) || (global.activebuffer == 1)) {
			newbuffer=1-global.activebuffer;
		} else {
			newbuffer=0;
		}; // end else - if

		if (global.verboselevel >= 1) {
			fprintf(stderr,".");
			dotcounter++;

			if (dotcounter > 80) {
				fprintf(stderr,"\n");
				dotcounter=0;
			}; // end if
		}; // end if
		
		// PCM data is located just after the DSTK-header
		// no other headers included
		memcpy(global.buffer[newbuffer],pcmdata,320);

		// MUTEX code to be added
		global.activebuffer=newbuffer;
		global.datatosend=1;

	} else {
		if (global.verboselevel >= 1) {
			fprintf(stderr,"X");
			dotcounter++;

			if (dotcounter > 80) {
				fprintf(stderr,"\n");
				dotcounter=0;
			}; // end if
		}; // end if
	}; // end if

}; // end while (forever)

// outside "forever" loop: we should never get here unless something went wrong

exit(0);
}; // end main


////////////////// END MAIN //////////////////////////
//////////////////////////////////////////////////////

// function senddata ////////////////////////////////
/////////////////////////////////////////////////////
static void funct_senddata () {
	static short int noiseorsilence[160*NOISEFRAMES];
	static int init=0;
	ssize_t nsample;
	static int noisecounter=0;

	static int audiosend;


	// function init
	if (init == 0) {
		struct timeval now;
		struct timezone tz;
		int loop;

		// randomise, only needed for noise
		if (global.addnoise) {
			gettimeofday(&now,&tz);
			srandom(now.tv_usec);

			for (loop=0;loop<160*NOISEFRAMES;loop++) {
				// create noise 

				int i=random();

				if (random() & 0x1) {
					noiseorsilence[loop]=(short) i & NOISEVOL;
				} else {
					noiseorsilence[loop]=(short) -i & NOISEVOL;
				}; // end else - if
			}; // end  for
		} else if (global.addsilence) {
			// silence, make everything zero
			memset((void *)&noiseorsilence,0,160*NOISEFRAMES);
		};  // end if

		init=1;
	}; // end if

	audiosend=0;

	// is there something to send?
	if (global.datatosend) {
		nsample=write(1,global.buffer[global.activebuffer],320);
		global.datatosend=0;
		audiosend=1;
	} else if ((global.addsilence) || (global.addnoise)){
		// nothing to send: send noise
		nsample=write(1,&noiseorsilence[160*noisecounter],320);

		noisecounter++;
		if (noisecounter >= NOISEFRAMES) {
			noisecounter=0;
		}; // end if
		audiosend=1;
	}; // end if

	if ((audiosend) && (nsample < 320)) {
		if (global.verboselevel >= 1) {
			fprintf(stderr,"Error: could not write 320 octets to stdout. Wrote %d. (%s)\n",(int)nsample,strerror(errno));
		}; // end if
	}; // end if
			

}; // end funct_senddata
