/////////////////////////////////////////////////////////////
// /////////////////////////////////////////////////////////
// function serialsend: called by timer every 20 ms
static void funct_serialsend (int sig) {
// static variables
// As this function is restarted every 20 ms, the information in the
// local variables is lost, except when the variable is defined as
// static

// other variables
int ret;

// sending pcm-packet (to keep the dvdongle busy)
ret=write (global.donglefd,&pcmbuffer_silent,322);

// this function also is used for the "inbound_timeout" function.
// decrease by one every 20 ms
if (global.inbound_timeout > 0) {
	global.inbound_timeout--;
}; // end if

// is there a valid dvframe?
// if yes, look at ringbuffer
// if not, send dummy ambe-frame
if (global.inbuffer_low == global.inbuffer_high) {
	// no valid frame or nothing in the buffer: send pseudo ambe-frame and set serial frame-valid to 0
	ret=write(global.donglefd,ambe_configframe,50);
	global.serframe_valid=0;
} else {
	// valid frame on inbound ringbuffer
	int buffernext;
	// tempory var
	static ambe_bufferframe_str *p;


	buffernext=global.inbuffer_low+1;
	if (buffernext >= INBUFFERSIZE) {
		buffernext=0;
	}; // end if


	p=global.inbuffer[buffernext];

	// copy the 9 octets ambe-data from inbuffer to the 26th to 35th
	// octet of ambe_validframe
	memcpy(&ambe_validframe[26],p->dvdata,9);
#if DEBUG >= 1
fprintf(stderr,"S G=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X \n",ambe_validframe[25],ambe_validframe[26],ambe_validframe[27],ambe_validframe[28],ambe_validframe[29],ambe_validframe[30],ambe_validframe[31],ambe_validframe[32],ambe_validframe[33],ambe_validframe[34]);
#endif

	// also copy structure to global structure to pass on DATK structures
	memcpy(&global.lastambeframe,p,sizeof(ambe_bufferframe_str));

	// write ambe frame to dongle and increase low-pointer
	ret=write(global.donglefd,ambe_validframe,50);
	global.inbuffer_low=buffernext;
	
	global.serframe_valid=1;
}; 

}; // end function funct_serialsend
