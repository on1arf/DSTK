// d_initdongle.h

// initdvdongle: initialises DVdongle: return socket-number to dongle on success, -1 on failure

// The function is part of the DATK (Dstar Audio ToolKit), a package for manipulating D-STAR audio
// and other streams inside a i-com based D-STAR repeater

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


int initdongle (char * dongledevice) {

// termios: structure to configure serial port
struct termios tio;

// other vars
int ret;
int donglefd;
int status;

// inbound buffer for serial port: here used locally in this function
unsigned char serialinbuff[8192];


// open serial port to dongle
donglefd=open(dongledevice, O_RDWR | O_NONBLOCK);

if (donglefd == 0) {
	fprintf(stderr,"DVdongle open failed! \n");
	return(-1);
}; // end if

// make exclusive lock
ret=flock(donglefd,LOCK_EX | LOCK_NB);

if (ret != 0) {
	int thiserror=errno;

	fprintf(stderr,"Exclusive lock to DVdongle failed. (%s) \n",strerror(thiserror));
	fprintf(stderr,"Dongle device not found or already in use\n");
	return(-1);
}; // end if


// now configure serial port
// Serial port programming for Linux, see termios.h for more info
memset(&tio,0,sizeof(tio));
tio.c_iflag=0; tio.c_oflag=0; tio.c_cflag=CS8|CREAD|CLOCAL; // 8n1
tio.c_lflag=0; tio.c_cc[VMIN]=1; tio.c_cc[VTIME]=0;

cfsetospeed(&tio,B230400); // 230 Kbps
cfsetispeed(&tio,B230400); // 230 Kbps
tcsetattr(donglefd,TCSANOW,&tio);



// First part of the program: initialise the dongle
// This runs pure sequencial

// main loop:
// senario:
// status 0: send "give-name" command to dongle
// status 1: wait for name -> send "start" command
// status 2: wait for confirmation of "start" -> send ambe-packet (configures decoder) + dummy pcm-frame

// init some vars
status=0;

if (global.verboselevel >= 1) {
	fprintf(stderr,"Initialising DVdongle\n");
};


while (status < 3) {
// some local vars

	uint16_t plen_i; 
	uint16_t ptype;

	if (global.verboselevel >= 1) {
		fprintf(stderr,"status = %d\n",status);
	}

	// read next frame, no error-correction, 2 ms of delay if no data
	// timeout is 4 seconds (2 ms * 2000) in status 3 (after sending
	// first command to dongle) or 1 (2 ms * 500) seconds otherwize
	if (status == 1) {
		ret=serialframereceive(donglefd,serialinbuff,&ptype,&plen_i,0,2000,2000);
	} else {
		ret=serialframereceive(donglefd,serialinbuff,&ptype,&plen_i,0,2000,500);
	}; // end else - if


	// part 1: what if no data received ???	
	if (ret < 0) {
	// nothing received.

		// check for status
		if (status == 1) {
			// 4 second timeout after first command to dongle.
			// are you sure there is a dongle present?
			fprintf(stderr,"Error: timeout receiving data from dongle.! Exiting\n");
			return(-1);
		}; // end if


		if (status == 0) {
		// status is 0 (after 1 second of waiting, send "request name")
#if DEBUG > 0
			printf("sending request name\n");
#endif
			ret=write (donglefd,request_name,4);
			// move to status 1
			status=1;
		}; // end if

	} else {
		// part2: data is received

#if DEBUG > 0
		printf("plen_i = %d, ptype = %d \n",plen_i, ptype);
#endif

#if DEBUG > 0
		int loop;
		// dump packet
		for (loop=0;loop<plen_i;loop++) {
			printf("%x ",serialinbuff[loop]);
		}; // end for
		printf("\n");
#endif

		// parse received packet:
		// case: status 2 in senario
		// reply to start received -> send initial ambe-packet
		if ((status == 2) && (plen_i == 3) && (ptype == 0)) {
#if DEBUG > 0
			printf("sending initial ambe-frame\n");
#endif
			// also send a dummy pcm-frame
			ret=write (donglefd,(void *) &pcmbuffer,322);

			ret=write (donglefd,ambe_configframe,50);

			// move to status 3
			status=3;
		}; // end if

		// case: status 1 in senario:
		// dongle name received -> send "start" command
		if ((status == 1) && (plen_i == 12) && (ptype == 0) && (serialinbuff[0] == 1) && (serialinbuff[1] == 0)) {
#if DEBUG > 0
			printf("sending start\n");
#endif
			ret=write (donglefd,command_start,5);
			// move to status 2
			status=2;
		}; // end if


	}; // end else - if (data received or not?)

}; // end while (status < 3)


if (global.verboselevel >= 1) {
	fprintf(stderr,"status = %d\n",status);
}

return (donglefd);

}; // end function
