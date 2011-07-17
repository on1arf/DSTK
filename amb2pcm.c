/* amb2pcm.c */

// amb2pcm: join AMBE-stream, decode to PCM and stream out as PCM stream
// Usage: cap2mc [-dng device] [-v ] [-g gain] [-si ipaddress ] [ -sp port] [-di ipaddress ] [ -dp port ] module [module ..]
// Usage: cap2mc -V

// this program requires a DVdongle to do AMBE decoding, connected to
// a USB-port

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
// 9 jan. 2011: version 0.0.1. Initial release

#define VERSION "0.0.1"
#define AMB2PCM

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

// to get rid of annoying warning about strndump
#define __USE_GNU
#include <string.h>
#include <stdint.h>

// term io: serial port
#include <termios.h>

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

// header-files for ip, udp and other
#include <linux/ip.h>
#include <linux/udp.h>

// needed for gettimeofdat (used with srandom)
#include <sys/time.h>

// change this to 1 for debugging
#define DEBUG 0

// must the pcmout function always send a packet even if no PCM data
// received. In that case, it will send a silent PCM-frame
#define ALWAYSSEND 0

// dstk.h
// global structures for DSTK
#include "dstk.h"

// amb2pcm.h: data structures + global data
// As cap2mc is multithreaded, global data-structures are used
// by the threads to communicate with each other
#include "amb2pcm.h"

// multicast.h
// functions deals with setting up multicast sockets
#include "multicast.h"

// serialframe.h
// functions to receive frames from DVdongle
#include "serialframe.h"

// d_initdongle.h: contains function with that name
#include "d_initdongle.h"

// d_ambein.h: contains the function with that name
// Is started as a seperate thread by the main function
#include "d_ambein.h"

// d_seriansend.h: contains the function with that name
// Is started every 20 ms to send data to dongle
#include "d_serialsend.h"

// d_serialreceive.h: contains the function with that name
// Is started as a seperate thread by the main function
#include "d_serialreceive.h"

// d_pcmout.h: contains function with that name
// Is started every 20 ms
#include "d_pcmout.h"



// defines for timed interrupts
#define CLOCKID CLOCK_REALTIME
#define SIG SIGRTMIN
#define SIG2 SIGRTMIN+1


//////////////////////////////////////////////////////////////////////////
// configuration part // // configuration part // // configuration part //
//////////////////////////////////////////////////////////////////////////
char * default_dongledevice="/dev/ttyUSB0";

char * default_s_ipaddress="ff11::4200";
int default_s_port=4200;
char * default_d_ipaddress="ff11::4300";
int default_d_port=4300; 

int default_ambegain=192;
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


int main(int argc,char** argv) {

// data structures:

int ambegain;

			
// posix threads
pthread_t thr_serialreceive;
pthread_t thr_ambein;


// vars for timed interrupts
struct sigaction sa1, sa2;
struct sigevent sev1, sev2;
timer_t timerid1, timerid2;
struct itimerspec its1, its2;

// some miscellaneous vars:
int ret;

int numinputmodule;

// loops
int paramloop;
int loop;

char * dongledevice;

// time structures: used with srandom
struct timeval now;
struct timezone tz;

// ////// data definition done ///


// main program starts here ///
// part 1: initialise vars and check cli-arguments

global.verboselevel=0;
dongledevice=default_dongledevice;
global.s_ipaddress=default_s_ipaddress; global.s_port=default_s_port;
global.d_ipaddress=default_d_ipaddress; global.d_port=default_d_port;
global.allmodule=0;
global.repeatermodactive[0]=0;global.repeatermodactive[1]=0;global.repeatermodactive[2]=0;
global.reflectormodactive[0]=0;global.reflectormodactive[1]=0;global.reflectormodactive[2]=0;
numinputmodule=0;
ambegain=default_ambegain;


// create streamid1: random 16 bit value
// streamid is random
gettimeofday(&now,&tz);
srandom(now.tv_usec);

global.globalstreamid=random();


// allocate memory for buffers
for (loop=0;loop<INBUFFERSIZE;loop++) {
	global.inbuffer[loop]=(ambe_bufferframe_str*) malloc(sizeof(ambe_bufferframe_str));
	if (global.inbuffer[loop] == NULL) {
		fprintf(stderr,"ERROR: could not allocated for INBUFFER ELEMENT %d\n",loop);
	}; // end if
}; // end for

// the same of the outbuffer. Already fill in version and fieldif
for (loop=0;loop<OUTBUFFERSIZE;loop++) {
	global.outbuffer[loop]=(mcframe_pcm_str*) malloc(sizeof(mcframe_pcm_str));
	if (global.outbuffer[loop] == NULL) {
		fprintf(stderr,"ERROR: memory could not allocated for OUTBUFFER ELEMENT %d\n",loop);
	}; // end if

	// fill in fixed parts of the structure
	global.outbuffer[loop]->dstkheader.version=1;
	global.outbuffer[loop]->dstkheader.flags=0 | DSTK_FLG_LAST; // in this version of this allocation, we only
														// have one single dstk subframe in a dstk superframe
	global.outbuffer[loop]->dstkheader.type=htonl(TYPE_PCM_PCM);
	global.outbuffer[loop]->dstkheader.streamid1=htons(global.globalstreamid); // fixed for the duration of this application
	global.outbuffer[loop]->dstkheader.seq2=0; // seq2 not used
	global.outbuffer[loop]->dstkheader.size=htons(PCMBUFFERSIZE+sizeof(uint32_t)); // size
	global.outbuffer[loop]->dstkheader.undef53=0; // undefined -> make 0
	global.outbuffer[loop]->dstkheader.undef54=0; // undefined -> make 0
}; // end for




// CLI option decoding
// format: c_ambedecode [-dng device] [-v ] [-g gain] [-si ipaddress ] [ -sp port] [ -di ipaddress] [ -dp port ] [-am || module [module ..] ]

for (paramloop=1;paramloop<argc;paramloop++) {
	char * thisarg=argv[paramloop];

	if (strcmp(thisarg,"-dng") == 0) {
		// -dng = dongledevice
		if (paramloop+1 < argc) {
			paramloop++;
			dongledevice=argv[paramloop];
		}; // end if
	} else if (strcmp(thisarg,"-V") == 0) {
		// -V = version
			fprintf(stderr,"%s version %s\n",argv[0],VERSION);
			exit(0);
	} else if (strcmp(thisarg,"-h") == 0) {
		// -h = help
			help(argv[0]);
			exit(0);
	} else if (strcmp(thisarg,"-g") == 0) {
	// -g = ambe gain
		if (paramloop+1 < argc) {
			paramloop++;
			ambegain=atoi(argv[paramloop]);
		}; // end if
	} else if (strcmp(thisarg,"-v") == 0) {
		// -v = verbose
		global.verboselevel++;
	} else if (strcmp(thisarg,"-si") == 0) {
		// -li = LISTEN ipaddress
		if (paramloop+1 < argc) {
			paramloop++;
			global.s_ipaddress=argv[paramloop];
		}; // end if
	} else if (strcmp(thisarg,"-sp") == 0) {
		// -li = LISTEN port
		if (paramloop+1 < argc) {
			paramloop++;
			global.s_port=atoi(argv[paramloop]);
		}; // end if
	} else if (strcmp(thisarg,"-di") == 0) {
		// -li = STREAM ipaddress
		if (paramloop+1 < argc) {
			paramloop++;
			global.d_ipaddress=argv[paramloop];
		}; // end if
	} else if (strcmp(thisarg,"-dp") == 0) {
		// -li = STREAM port
		if (paramloop+1 < argc) {
			paramloop++;
			global.d_port=atoi(argv[paramloop]);
		}; // end if
	} else if (strcmp(thisarg,"-am") == 0) {
		// -am = all modules
		global.allmodule=1;
	} else {
		// modules can be "a" to "c" (for repeater modules) or "ra" to "rc" (for reflector modules)
		int refl=0;
		char mod; int mod_i;

		if ((thisarg[0] == 'r') || (thisarg[0] == 'R')) {
			refl=1;
			mod=thisarg[1];
		} else {
			mod=thisarg[0];
		}; // end else - of


		mod_i=-1;
		if ((mod == 'a') || (mod == 'A')) {
			mod_i=0;
		} else if ((mod == 'b') || (mod == 'B')) {
			mod_i=1;
		} else if ((mod == 'c') || (mod == 'C')) {
			mod_i=2;
		};

		if (mod_i < 0) {
			// invalid argument
			fprintf(stderr,"Warning: unknown module: %c \n",mod);
		} else {
			if (refl) {
				global.reflectormodactive[mod_i]=1;
			} else {
				global.repeatermodactive[mod_i]=1;
			}; // end else - if
		}; // end else - if
	}; // end elsif - elsif - elsif - if

}; // end for

// We should have either "allmodule" set, or at least one module
if (global.allmodule + global.repeatermodactive[0] + global.repeatermodactive[1] + global.repeatermodactive[2] +
global.reflectormodactive[0] + global.reflectormodactive[1] + global.reflectormodactive[2] == 0) {
	fprintf(stderr,"Error: No module chosen and \"allmodule\" option not set!\n");
	usage(argv[0]);
	exit(-1);
}; // end if

// output AMBE gain must be between 1 and 255
if ((ambegain  <= 1)  || (ambegain > 255)) {
	fprintf(stderr,"Error: AMBEgain must be between 1 and 255 (default = 192)!\n");
	usage(argv[0]);
	exit(-1);
}; // end if

// initialise pcmbuffer: always 320 bytes (20 ms, 8000 sps, 16 bit/sample)
// buffsize (0x8142) actualy contains 2 data-items:
// - first 5 bits: "ptype" (here: '1000 0')
// - remaining 13 bites: length: 0x142 = 322 (320 databytes + 2 header bytes)
pcmbuffer.buffsize1=0x42;
pcmbuffer.buffsize2=0x81;
memset(&pcmbuffer.pcm,0,320);

pcmbuffer_silent.buffsize1=0x42;
pcmbuffer_silent.buffsize2=0x81;
memset(&pcmbuffer_silent.pcm,0,320);

// overwrite AMBE gain in ambe frame
ambe_configframe[25]=(unsigned char) ambegain;
// copy empty "configuration frame" onto ambe frame
memcpy(ambe_validframe,ambe_configframe,50);


global.donglefd=initdongle(dongledevice);


// Part 2 of main program

// now we are at status level 3. We have just received the acknowledge
// to the "start" and have send the 1st ambe-packet to configure
// the encoder

// next steps
// - start the "ambe_in" thread
// - the "send data" function is started via a interrupt-time every
// 20 ms, reads the data prepared by ambe_in (if present) and sends it
// out to the dvdongle. Otherwize, it sends a dummy package containing
// all silence
// - start the "serialreceive" thread that receives the frames from
// the dvdongle and prepares it for multicastsend
// - the "multicastsend" looks for data from serialreceive and sends a
// UDP-package to a multicast address. If no packets are present, a
// dummy package containing all silence is send


//// Threads started as normal "pthread" 
// init and start thread ambein
// MULTICAST CAPTURE: at this time, does not do anything
// just set "framevadid" to 0 and then go to sleep
pthread_create(&thr_ambein,NULL,funct_ambein, (void *) &global);

// init and start thread serialreceive
// SERIALRECEIVE
pthread_create(&thr_serialreceive,NULL,funct_serialreceive, (void *) &global);


//// threads started as timed-interrupt threads

// establing handler for signal 
sa1.sa_flags = 0;
sa1.sa_handler = funct_serialsend;
sigemptyset(&sa1.sa_mask);

ret=sigaction(SIG, &sa1, NULL);
if (ret <0) {
	fprintf(stderr,"error in sigaction!\n");
	exit(-1);
}; // end if


/* Create the timer */
sev1.sigev_notify = SIGEV_SIGNAL;
sev1.sigev_signo = SIG;
sev1.sigev_value.sival_ptr = &timerid1;

ret=timer_create(CLOCKID, &sev1, &timerid1);
if (ret < 0) {
	fprintf(stderr,"error in timer_create!\n");
	exit(-1);
}; // end if


// start timed function, every 20 ms
its1.it_value.tv_sec = 0;
its1.it_value.tv_nsec = 5000000; // 5 ms: start after 1/4 of first frame
its1.it_interval.tv_sec = its1.it_value.tv_sec;
its1.it_interval.tv_nsec = 20000000; // 20 ms = 20 million nanoseconds

ret=timer_settime(timerid1, 0, &its1, NULL);
if (ret < 0) {
	fprintf(stderr,"error in timer_settime!\n");
	exit(-1);
}; // end if

// establing handler for signal 
sa2.sa_flags = 0;
sa2.sa_handler = funct_pcmout;
sigemptyset(&sa2.sa_mask);

ret=sigaction(SIG2, &sa2, NULL);
if (ret <0) {
	fprintf(stderr,"error in sigaction!\n");
	exit(-1);
}; // end if


/* Create the timer */
sev2.sigev_notify = SIGEV_SIGNAL;
sev2.sigev_signo = SIG2;
sev2.sigev_value.sival_ptr = &timerid2;

ret=timer_create(CLOCKID, &sev2, &timerid2);
if (ret < 0) {
	fprintf(stderr,"error in timer_create!\n");
	exit(-1);
}; // end if


// start timed function, every 20 ms
its2.it_value.tv_sec = 0;
its2.it_value.tv_nsec = 15000000; // 5 ms: start after 1/4 of first frame
its2.it_interval.tv_sec = its2.it_value.tv_sec;
its2.it_interval.tv_nsec = 20000000; // 20 ms = 20 million nanoseconds

ret=timer_settime(timerid2, 0, &its2, NULL);
if (ret < 0) {
	fprintf(stderr,"error in timer_settime!\n");
	exit(-1);
}; // end if


// main thread ends here
while (forever) {
	// in theory, this program never ends. So we have an endless loop with just "sleep"
	sleep(60);
}; // end while


exit(0);
}; // end main

