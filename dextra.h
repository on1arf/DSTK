// dextra linking and heartbeats

// release-info:
// 25 Mar. 2011: version 0.2.1: Initial release

// dextralink: init link to dextra gateway or reflector
int dextralink(int outsock, char * callsign, int calllen, struct sockaddr_in6 * serverAddress, int destport) {

// linkmessage is "MYCALL  D"
unsigned char linkmsg[] = "        D";

int missed;
int ret;

// MTU of ethernet, so we should not receive anything longer
// so we should be OK
unsigned char recbuffer[ETHERNETMTU];


// sanity check
if (serverAddress == NULL) {
	fprintf(stderr,"Error in dextralink: invalid serverAddress \n");
	exit(-1);
}; // end if

// destport was created in main thread
serverAddress->sin6_port = htons((unsigned short int) destport);  

// sanity check
if (calllen > 6) {
	fprintf(stderr,"Error in dextralink: MYCALL is longer then 6 characters. Shouldn't happen! \n");
	return(-1);
}; // end if

// linkmessage is "MYCALL  D"
// copy call
memcpy(linkmsg,callsign,calllen);

ret=sendto(outsock, linkmsg, 9,0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));

if (ret== -1 ) {
	fprintf(stderr,"Error: error during sending udp-packet for dextra linking!\n");
	exit(-1);
}; // end if

// OK, first packet was send successfull: send 4 more
ret=sendto(outsock, linkmsg, 9,0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));
ret=sendto(outsock, linkmsg, 9,0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));
ret=sendto(outsock, linkmsg, 9,0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));
ret=sendto(outsock, linkmsg, 9,0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));

missed=0;

// wait for reply: a UDP message of 9 octets: gateway/reflector callsign + \0

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

	// data received, is it 9 characters, otherwize ignore the package
	// also, last character should be a 0
	if ((ret == 9) && (recbuffer[8] == 0)) {
		// found it
		if (global.verboselevel >= 1) {
			fprintf(stderr,"Link established to %s\n",recbuffer);
		}; // end if
		break;
	}; // end if

}; // end while

// if we missed less then 5 packets, it is concidered a success
if (missed <= 5) {
	return(0);
} else {
	return(-1);
}; // end if

}; // end function

/// end function ///



///////////////////////////////////////////////////
///////////////////////////////////////////////////
// function dextra heartbeat
// started as a posix thread


// this function also serves as timeout check for streams in the cache

static void * funct_dextraheartbeat (void * in_pglobal) {

globaldata_str * pglobal = (globaldata_str *) in_pglobal;


// heartbeat message is "MYCALL  \0"
unsigned char heartbeatmsg[] = "        \0";

int ret;
int counter=6;
int loop;

static struct sockaddr_in6 * serverAddress;


// ai_addr was created in main thread
serverAddress = (struct sockaddr_in6 *) global.ai_addr;

if (serverAddress == NULL) {
	fprintf(stderr,"Error: main thread does not give valid a_addr \n");
	exit(-1);
}; // end if

// destport was created in main thread
serverAddress->sin6_port = htons((unsigned short int) global.destport);  

// linkmessage is "MYCALL  \0"
memcpy(heartbeatmsg,global.mycall,6);

// endless loop
while (forever) {
	// we start with a 6 second delay
	sleep(1);

	counter--;

	// dextra heartbeat: send every 6 seconds
	if (counter <= 0) {
		counter=6;

		ret=sendto(global.outsock, heartbeatmsg, 9,0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));

		if (ret == -1) {
			if (pglobal->verboselevel >= 1) {
				fprintf(stderr,"Warning: sending udp-packet for dextra heartbeat failed! (error = %s)\n",strerror(errno));
			}; // end if
			continue;
		}; // end if

		// OK, sending the first packet went OK, let's send the remaining 4
		sendto(global.outsock, heartbeatmsg, 9,0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));
		sendto(global.outsock, heartbeatmsg, 9,0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));
		sendto(global.outsock, heartbeatmsg, 9,0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));
		sendto(global.outsock, heartbeatmsg, 9,0,(struct sockaddr *) serverAddress, sizeof(struct sockaddr_in6));
	}; // end if

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

}; // end while (loop forever);

// outside loop, we should never come here

fprintf(stderr,"Error in dextra heartbeat: dropped out of endless loop. Shouldn't happen");

}; // end function
