
/////////////////////////////////////////////////////////////
// /////////////////////////////////////////////////////////
// function pcmout: called by timer every 20 ms



static void funct_pcmout (int sig) {

// static variables
// As this function is restarted every 20 ms, the information in the
// local variables is lost, except when the variable is defined as
// static

// silent frame
static mcframe_pcm_str mcframe_silent;

// some other vars
static int init=0;
static uint16_t sequence=0;
static int ret;


// outgoing udp stream
static int sock_out;
static struct sockaddr_in6 MulticastOutAddr;


if (init == 0) {
	// this part is started the first time this function gets called

	init=1;


	// open a UDP socket and store information in the global data structure
	sock_out=open_for_multicast_out(global.verboselevel);

	if (sock_out < 0) {
		fprintf(stderr,"Error in open_for_multicast_out! Exiting!\n");
		exit(-1);
	}; // end if

fprintf(stderr,"Streaming to addr %s poort %d \n",global.s_ipaddress, global.s_port);

	// packets are send to multicast address, UDP port 40000 or 40001
	MulticastOutAddr.sin6_family = AF_INET6; 
	MulticastOutAddr.sin6_scope_id = 1;
	MulticastOutAddr.sin6_port = htons((unsigned short int) global.s_port);

	ret=inet_pton(AF_INET6,global.s_ipaddress,(void *)&MulticastOutAddr.sin6_addr);
	if (ret != 1) {
		fprintf(stderr,"Error: could not convert %s into valid address. Exiting\n",global.s_ipaddress);
		exit(1);
	}; // end if




	// fill in the fixed parts of this structure
	mcframe_silent.dstkheader.version=1; // currently only version 1 is defined
	mcframe_silent.dstkheader.flags=1; // currently only version 1 is defined
	mcframe_silent.dstkheader.origin=htons(ORIGIN_FILLER); // origin is not defined
	mcframe_silent.dstkheader.type=htonl(TYPE_PCM_FLL); // type 2: silent frame
	mcframe_silent.dstkheader.seq2=0; // sequence 2 is not used
	mcframe_silent.dstkheader.streamid1=htons(global.globalstreamid); // streamid1
	mcframe_silent.dstkheader.streamid2=0; // streamid2 is 0 for silence frames
	mcframe_silent.dstkheader.size=htons(PCMBUFFERSIZE+sizeof(int)); // only 1 subframe per superframe in
														// this version of this program
	memset(&mcframe_silent.pcm,0,320); // fill up PCM part with all 0

}; // end if (init)


// is there something to send?
if (global.sndframe_valid == 1) {
		// yes

	// but is there still something in the buffer
	if (global.outbuffer_high != global.outbuffer_low) {
		int buffernext;

		buffernext=global.outbuffer_low+1;

		if (buffernext >= OUTBUFFERSIZE) {
			buffernext=0;
		}; // end if

		// fill in variable part of DSTK-header
		global.outbuffer[buffernext]->dstkheader.seq1=sequence; 
		sequence++;

		// send multicast packet
		ret=sendto(sock_out, global.outbuffer[buffernext], sizeof(mcframe_pcm_str), 0, (struct sockaddr *) &MulticastOutAddr, sizeof(MulticastOutAddr));

		if (ret < 0) {
			//  error
			fprintf(stderr,"Warning: sendto fails (error = %d(%s))\n",errno,strerror(errno));
		}

// MUTEX CODE NEEDED
	global.outbuffer_low=buffernext;

	} else {
		// nothing to send anymore. Set send-frame valid to 0 again
		global.sndframe_valid=0;
	}; // end else - if
}; // end if

// if not valid and alwayssend set to 1, send dummy (silent) frame
if ((global.sndframe_valid == 0) && (ALWAYSSEND == 1)) {
	// fill in variable part of DSTK header
	mcframe_silent.dstkheader.seq1=sequence; 
	sequence++;

	// send multicast packet
	ret=sendto(sock_out, &mcframe_silent, sizeof(mcframe_pcm_str), 0, (struct sockaddr *) &MulticastOutAddr, sizeof(MulticastOutAddr));

	if (ret < 0) {
		// error
		fprintf(stderr,"udp packet could not be send!\n"); 
	}
}; // end if


}; // end function func_udpsend


