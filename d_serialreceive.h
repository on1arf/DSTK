// //////////////////////////////////////
// //////////////////////////////////////
// function serial_port receive        //
// //////////////////////////////////////
// This process receives data from the //
// serial port and save it in a file   //
// It is run as a thread               //
// //////////////////////////////////////


void * funct_serialreceive (void * globaldatain) {
globaldatastr * pglobal;

// pointer to allpacketsend -> data returned from
// thread to main program

unsigned char serialinbuff[8192];

// import data from global data
pglobal = (globaldatastr *) globaldatain;

// mark thread as started
pglobal->welive_serialreceive = 1;


while (forever) {
	uint16_t plen_i;
	uint16_t ptype;

	int ret;

	// get next frame, correction is enabled, 2 ms of delay if no data
	// and 1 second timeout (500 times 2 ms)
	ret=serialframereceive(global.donglefd,serialinbuff,&ptype,&plen_i,1,2000,500);


	// we are only interested in PCM data (ptype = 4, length = 320 bytes)
	if ((ptype == 4) && (plen_i == 320)) {
		// write data in ringbuffer

		// only do this if the PCM frame that has been received contains REAL data

		if (pglobal->serframe_valid == 1) {

			// if there still place to store frame?
			int buffernext;

			buffernext=pglobal->outbuffer_high+1;
			if (buffernext >= OUTBUFFERSIZE) {
				buffernext=0;
			}; // end if

			// store if still place
			if (buffernext != pglobal->outbuffer_low) {
				// temp var
				static mcframe_pcm_str *p;
				uint32_t tmp_type;

				p=pglobal->outbuffer[buffernext];

				memcpy(&p->pcm,serialinbuff,320);

#if DEBUG > 1
{
int l;
fprintf(stderr,"OB: ");
for (l=0;l<320;l++) {
	fprintf(stderr,"%02X ",serialinbuff[l]);
}; // end for
fprintf(stderr,"\n");
}; // end
#endif

				// fill in other data
				p->dstkheader.origin=htons(global.lastambeframe.origin);
				p->dstkheader.streamid2=htons(global.lastambeframe.streamid);

				tmp_type=global.lastambeframe.type;

				if (global.lastambeframe.direction) {
					tmp_type |= TYPEMASK_FLG_DIR;
				}; // end if
				
				if (global.lastambeframe.first) {
					tmp_type |= TYPEMASK_FLG_FIRST;
				}; // end if
				
				if (global.lastambeframe.last) {
					tmp_type |= TYPEMASK_FLG_LAST;
				}; // end if
				
				p->dstkheader.type=htonl(tmp_type);


				// increase output buffer
				pglobal->outbuffer_high=buffernext;

				// mark as valid if we have at least 20 good frames
				if (pglobal->sndframe_valid == 0) {
					int buffdiff;

					if (pglobal->outbuffer_high > pglobal->outbuffer_low) {
						buffdiff=pglobal->outbuffer_high - pglobal->outbuffer_low;
					} else {
						buffdiff=pglobal->outbuffer_low + OUTBUFFERSIZE - pglobal->outbuffer_high;
					}; // end else - if

					if (buffdiff >= 20) {
						pglobal->sndframe_valid=1;
					}; // end if
				}; // end if
			} else {
		     if (pglobal->verboselevel >= 1) {
      	   fprintf(stderr,"Warning: could not store pcm-data in outbuffer!\n");
      		}; // end if
			}; // end else - if
			

		} else {
			// not a valid frame. ignore it

			// If the buffersize was less then 5 and never used, reset the pointers
			if (pglobal->sndframe_valid == 0) {
				int buffdiff;

// MUTEX CODE NEEDED
				if (pglobal->outbuffer_high > pglobal->outbuffer_low) {
					buffdiff=pglobal->outbuffer_high - pglobal->outbuffer_low;
				} else {
					buffdiff=pglobal->outbuffer_low + OUTBUFFERSIZE - pglobal->outbuffer_high;
				}; // end else - if

				if (buffdiff < 20) {
					pglobal->outbuffer_low = pglobal->outbuffer_high;
				}; // end if
			}; // end if

		}; // end else - if

	}; // end if (ambe packet received)


}; // end while (forever)

// we should never get here

// mark thread as done
pglobal->welive_serialreceive = 0;

return(0);
}; // end if



