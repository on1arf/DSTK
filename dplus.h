// dplus linking and heartbeats

// release-info:
// 25 Mar. 2011: version 0.2.1: Initial release


// dplus commands are based on DVdongle commands. The first two
// octets contain the length (13 bits) + flags (3 bits) in
// reverse order
unsigned char dplus_init[] = {0x05, 0x00, 0x18, 0x00, 0x01};

unsigned char dplus_link[] = {0x1C, 0xC0, 0x04, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x56, 0x30, 0x31, 0x39, 0x39, 0x39, 0x37};
// octets 4 up to 11 contain callsign
// octets 20 tp to 27 contain serialnumber of DVdongle (here set fixed
// to DV019997).

// link OK message: text (last 4 octets): OKRW
unsigned char dplus_linkok[] = {0x08, 0xC0, 0x04, 0x00, 0x4F, 0x4B, 0x52, 0x57};


unsigned char dplus_heartbeat[] = {0x03, 0x60, 0x00};


// dpluslink: init link to dplus gateway or reflector
int dpluslink(int outsock, char * callsign, int calllen, struct sockaddr_in6 * serverAddress, int destport) {

int missed;
int ret;

// MTU of ethernet, so we should not receive anything longer
// so we should be OK
unsigned char recbuffer[ETHERNETMTU];


// sanity check
if (calllen > 6) {
	fprintf(stderr,"Error in dpluslink: MYCALL is longer then 6 characters. Shouldn't happen! \n");
	return(-1);
}; // end if

// sanity check
if (serverAddress == NULL) {
	fprintf(stderr,"Error in dpluslink: invalid serverAddress \n");
	exit(-1);
}; // end if


// destport was created in main thread
serverAddress->sin6_port = htons((unsigned short int) destport);  


// send init message
ret=sendto(outsock, dplus_init, sizeof(dplus_init),0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));

if (ret== -1 ) {
	fprintf(stderr,"Error: error during sending udp-packet for dplus linking!\n");
	exit(-1);
}; // end if


missed=0;

// wait for reply: which is the same as the init request

while (missed < 5) {
	ret=recv(outsock, recbuffer, ETHERNETMTU, MSG_DONTWAIT);

	if (ret == -1) {
	// nothing received
		missed++;

		if (missed < 5) {
			sleep(1);
			continue;
		}; // end if
	};

	// data received should the same as the init-message send
	// , otherwize ignore the package
	if ((ret == sizeof(dplus_init)) && (memcmp(recbuffer, dplus_init, sizeof(dplus_init))== 0)) {
		// found it
		if (global.verboselevel >= 1) {
			fprintf(stderr,"Init OK!\n");
		}; // end if
		break;
	}; // end if

}; // end while



if (missed >= 5) {
	// no init message received. Linking failed
	return(-1);
}; // init message failed


// init successfull: send link message

// linkmessage octets 4 up to 11 contain callsign
// copy call
memcpy(&dplus_link[4],callsign,calllen);

ret=sendto(outsock, dplus_link, sizeof(dplus_link),0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));

if (ret == -1 ) {
	fprintf(stderr,"Error: error during sending udp-packet for dplus linking!\n");
	exit(-1);
}; // end if


missed=0;
// wait for reply: dplus_linkOK

while (missed < 5) {
	ret=recv(outsock, recbuffer, ETHERNETMTU, MSG_DONTWAIT);

	if (ret == -1) {
	// nothing received
		missed++;

		if (missed < 5) {
			sleep(1);
			continue;
		}; // end if
	};

	// data received should link_OK message: 0x08,0xC0,0x04,0x00,"OKRW" 
	// otherwize ignore the package
	if ((ret == sizeof(dplus_linkok)) && (memcmp(recbuffer, dplus_linkok, sizeof(dplus_linkok))== 0)) {
		// found it
		if (global.verboselevel >= 1) {
			fprintf(stderr,"Linkrequest OK!\n");
		}; // end if
		break;
	}; // end if

}; // end while



if (missed >= 5) {
	// no init message received. Linking failed
	return(-1);
}; // init message failed
// all done, terug
return(0);

}; // end function

/// end function ///



///////////////////////////////////////////////////
///////////////////////////////////////////////////
// function streamcache timeout
// started as a posix thread


static void * funct_dplusonesecond (void * in_pglobal) {

int loop;


// endless loop
while (forever) {
	// cache timeout: executed every second
	for (loop=0; loop<MAXSTREAMACTIVE; loop++) {
		if (global.stream_timeout[loop]) {
			global.stream_timeout[loop]--;

			// has it now become zero
			// sanity check: also check negative values
			if (global.stream_timeout[loop] <= 0) {
				// sanity check: reset to zero
				global.stream_timeout[loop]=0;

				global.stream_active[loop]=0;
			}; // end if
		}; // end if

	}; // end for

	// sleep 1 second
	sleep(1);

}; // end while (loop forever);

// outside loop, we should never come here

fprintf(stderr,"Error in dplus cache timeout: dropped out of endless loop. Shouldn't happen");

exit(-1);

}; // end function
