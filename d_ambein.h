// //////////////////////////////////////
// //////////////////////////////////////
// function ambe IN                    //
// //////////////////////////////////////
// Capture DV-frames from multicast-   //
// stream and put them on ringbuffer   //
// to d_serialsend                     //
// //////////////////////////////////////


// functions defined further down:
int push_to_inbuffer ( unsigned char * data, uint16_t origin, uint32_t type,  uint16_t streamid, int direction, int first, int last, char * other, globaldatastr * pglobal);


void * funct_ambein (void * globaldatain) {
globaldatastr * pglobal;

// import data from global data
pglobal = (globaldatastr *) globaldatain;


// local vars
unsigned char receivebuffer[ETHERNETMTU];

// misc vars
int ret;

int packetsize;

struct dstar_dv_rf_header * this_dv_rf_header;
struct dstar_dv_header * this_dv_header;
struct dstar_dv_data * this_dv_data;
uint8_t this_sequence;

int activestatus; // status: 0 = no stream active, 1 = start frame received, 2 = DV frames received
int activedirection;

uint8_t activesequence;
u_short activestreamid;

int expectedsizeofpacket;
uint32_t thisframetype;

// other stuff
int missed;

char * texttoprint;

// networking vars
int sock_in;

// tempory buffer of last ambe frame
// (used during packetloss)
unsigned char ambe_last[9];

// vars to deal with DSTK frames;
dstkheader_str * dstkhead_in;

int reflector;

// vars to process streams
int direction;
int streamid;

// function starts here

// init vars
activestatus=0;

texttoprint=strndup("RPT1=        ,RPT2=        ,YOUR=        ,MY=        /    ",63);


// init function
pglobal->welive_mccapture=1;
// set receive-frame-valid to 0 to init
pglobal->rcvframe_valid=0;

// init ambe_last: fill with all 0
memset(&ambe_last,0,9);


sock_in=open_and_join_mc(pglobal->s_ipaddress , pglobal->s_port, pglobal->verboselevel);

fprintf(stderr,"listening on address %s, address %d \n",pglobal->s_ipaddress, pglobal->s_port);


streamid=0;
while (forever) {

//int loop;

	// some local vars
	int foundit;
	int giveup;
	int dstkheaderoffset;
	int otherfound;

	uint16_t origin;

	char module_c;
	int module_i;
	
	packetsize=recvfrom(sock_in,receivebuffer,ETHERNETMTU,0,NULL,0);

	if (packetsize == -1) {
		// no data received. Wait 2 ms and try again
		usleep(2000);
		continue;
	}; // end if

	if (packetsize <= 0) {
		fprintf(stderr,"Packetsize to small! \n");
		continue;
	}; // end if

	// check packet: we should find a DSTK frame
	foundit=0; giveup=0; dstkheaderoffset=0; otherfound=0;
//for (loop=0;loop<20;loop++) {
//	fprintf(stderr,"%02X ",receivebuffer[loop]);
//}; // end for
//fprintf(stderr,"\n");

	while ((! foundit) && (! giveup)) {
		// check DSTK packet header
		dstkhead_in = (dstkheader_str *) (receivebuffer + dstkheaderoffset);
#if DEBUG >= 1
fprintf(stderr,"type = %08X, after filter = %08X, should have %08X \n",ntohl(dstkhead_in->type),ntohl(dstkhead_in->type) & TYPEMASK_FILT_GRP, TYPE_AMB);
#endif

		if (dstkhead_in->version != 1) {
			fprintf(stderr,"DSTK header version 1 expected. Got %d\n",dstkhead_in->version);
			giveup=1;
			break;
		} else if ((ntohl(dstkhead_in->type) & TYPEMASK_FILT_GRP) == TYPE_AMB) {
			// OK, found a DSTK subframe of the group TYPE_AMB
			foundit=1;
			break;
		}; // end elsif - if

		// OK, we found something, but it's not what we are looking for
		otherfound=1;


		if ( (! (dstkhead_in->flags | DSTK_FLG_LAST)) &&
			(dstkheaderoffset+ntohs(dstkhead_in->size)+sizeof(dstkheader_str) +sizeof(dstk_signature) <= packetsize )) {
			// not yet found, but there is a pointer to a structure further
			// on in the received packet
			// And it is still within the limits of the received packet

			// move up pointer
			dstkheaderoffset+=sizeof(dstkheader_str)+ntohs(dstkhead_in->size);

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
				fprintf(stderr,"Warning: received packet does not contain AMB sub-packet!\n");
			} else {
				fprintf(stderr,"Warning: received packet is not a DSTK packet!\n");
			}; // end else - if
		}; // end if
		continue;
	}; // end if

	// sanity-check: check if the packet that has been received is large
	// enough to contain all data (error-check on overloading limits)
	// we are currently at the beginning of a DSTK header, so the frame
	// should be at least large enough to hold the DSTK header + 29, 33 or
	// 58 octets (depending on the datatype)

	// in the mean time, the type-of-packet is checked
	expectedsizeofpacket=-1;
	thisframetype=ntohl(dstkhead_in->type) & TYPEMASK_FILT_NOFLAGS;

	if (thisframetype == TYPE_AMB_CFG) {
		expectedsizeofpacket=48;
	} else if (thisframetype == TYPE_AMB_DV) {
		expectedsizeofpacket=19;
	} else if (thisframetype == TYPE_AMB_DVE) {
		expectedsizeofpacket=22;
	}; // end elsif - elsif - if

	if (expectedsizeofpacket == -1) {
		if (pglobal->verboselevel >= 1) {
			fprintf(stderr,"Warning: received packet with unknown frame-type: %04X\n",thisframetype);
		}; // end if
	continue;
	}; // end if

	// check size
	if (packetsize < dstkheaderoffset + sizeof(dstkheader_str) + expectedsizeofpacket) {
		if (pglobal->verboselevel >= 1) {
			fprintf(stderr,"Warning, received packet not large enough to contain DSTK_HEADER + %d octets of DV content!\n",expectedsizeofpacket);
		}; // end if
		continue;
	}; // end if


	// set direction
	if (ntohl(dstkhead_in->type) & TYPEMASK_FLG_DIR) {
		direction=1; // outbound
	} else {
		direction=0; // inbound
	}; // end else - if

	// copy some other info
	origin=ntohs(dstkhead_in->origin); 

	// process package
	// go do dstar header

	// info: status: 0 = no DV stream active, 1 = stream active

	// what kind of packet it is?
	// is it a start-of-stream packet
	if (thisframetype == TYPE_AMB_CFG) {

		// check "STRHDR1" flag. If set, the AMBE-frames also contain a "DSTAR stream header1"
		// header which is not used here
		// but we DO need to jump over it

		if (ntohl(dstkhead_in->type) & TYPEMASK_FLG_AMB_STRHDR1) {
			// set pointer to DV-header
			this_dv_header = (struct dstar_dv_header *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct dstar_str_header1));

			// set pointer for RF-header (follows DV-header immediatly)
			this_dv_rf_header = (struct dstar_dv_rf_header *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct dstar_dv_header) + sizeof(struct dstar_str_header1));

		} else {
			// set pointer to DV-header
			this_dv_header = (struct dstar_dv_header *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str));

			// set pointer for RF-header (follows DV-header immediatly)
			this_dv_rf_header = (struct dstar_dv_rf_header *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct dstar_dv_header));

		}; // end else - if

                memcpy(&texttoprint[5],&this_dv_rf_header->rpt1_callsign,8);
                memcpy(&texttoprint[19],&this_dv_rf_header->rpt2_callsign,8);
                memcpy(&texttoprint[33],&this_dv_rf_header->your_callsign,8);
                memcpy(&texttoprint[45],&this_dv_rf_header->my_callsign,8);
                memcpy(&texttoprint[54],&this_dv_rf_header->my_callsign_ext,4);
                fprintf(stderr,"RPT: %s, direction = %d\n",texttoprint,direction);

		// accept it when
		// status is 0 (no DV-stream active)
		// or when timeout (additional check to deal with
		// stale sessions where we did not receive an "end-of-stream" packet

		// the "inbound timeout" value is "armed" with 150 in this function and decreased by
		// one in the "serialsend" function. As that function is started every 20 ms, the
		// session will timeout if no packets received after 3 second.

		if ((activestatus == 0) || (pglobal->inbound_timeout == 0)) {
			// determine module
			// also determine reflector
			reflector=0;

printf("origin = %d \n",origin);

			// this depends on where a stream has originated

			// case 1: a stream originated from a RPC-stream (intercepted traffic
			//				between a RPC and gateway-server)
			if (origin == ORIGIN_LOC_RPC) {
				// outbound: check last character or RPT2
				if (direction) {
					module_c=this_dv_rf_header->rpt2_callsign[7];
				} else {
				// for repeater/in, check last char of RPT1
					module_c=this_dv_rf_header->rpt1_callsign[7];
				}; // end else - if

			} else if ((origin & ORIGINMASK_GROUP) == ORIGIN_CNF) {
				// for dplus or dextra reflectors, check the "CNF_RPT1" flag
				if (ntohl(dstkhead_in->type) & TYPEMASK_FLG_AMB_CNF_RPT1) {
					// rpt1 flag set
					module_c=this_dv_rf_header->rpt1_callsign[7];
				} else {
					// rpt1 flag no set -> rpt2
					module_c=this_dv_rf_header->rpt2_callsign[7];
				}; // end else - if

				reflector=1;

			} else if ((origin & ORIGINMASK_GROUP) == ORIGIN_LOC) {
				// for locally generated stream (voice-announcements), check last char of RPT2
				module_c=this_dv_rf_header->rpt2_callsign[7];
			} else {
				if (global.verboselevel >= 1) {
					fprintf(stderr,"Error, don't know how to handle packet of origin %04X to determine direction!\n",origin); 
				}; // end if
				continue;
			}; // end if

			module_i=-1;
			if ((module_c == 'a') || (module_c == 'A')) {
				module_i=0; module_c='A';
			} else if ((module_c == 'b') || (module_c == 'B')) {
				module_i=1; module_c='B';
			} else if ((module_c == 'c') || (module_c == 'C')) {
				module_i=2; module_c='C';
			}; // end elsif - elsif - if

			if (module_i <0) {
				if (global.verboselevel >= 1) {
					fprintf(stderr,"Error: invalid module %c \n",module_c);
				}; // end if
				continue;
			}; // end if

			// if is a stream we are interesting in?
			if (! global.allmodule) {

				// if reflector, check reflector module active list
				// if repeater, check repeater module active list	
				if (((reflector) && (!global.reflectormodactive[module_i]))
					|| ((!reflector) && (!global.repeatermodactive[module_i]))) {
					if (pglobal->verboselevel >= 1) {
						if (reflector) {
							fprintf(stderr,"Warning, received stream-start from reflector module we are not subscribed to: %c!\n",module_c);
						} else {
							if (direction) {
								fprintf(stderr,"Warning, received outbound stream-start from repeater module we are not subscribed to: %c!\n",module_c);
							} else {
								fprintf(stderr,"Warning, received inbound stream-start to repeater module we are not subscribed to: %c!\n",module_c);
							}; // end else - if
						}; // end else - if
					}; // end if

					// ignore packet
					continue;
				}; // end if 
			}; // end if

			// copy streamid, set frame counter, set status and go to next packet
			if (pglobal->verboselevel >= 1) {
				fprintf(stderr,"NS%04X \n",this_dv_header->dv_stream_id);
			}; // end if

			activestreamid=this_dv_header->dv_stream_id;
			activedirection=direction;
			activesequence=0;
			activestatus=1;

			// set timeout value
			pglobal->inbound_timeout=150;

		} else {
			if (pglobal->verboselevel >= 1) {
				//fprintf(stderr,"Error: DV header received when DV voice-frame expected\n");
				fprintf(stderr,"DVH ");
			}; // end if
			continue;
		}; // end else
	} else {
		// DV-data received
		int thisfirst, thislast;

		// set pointer for rf_data
		// check "STRHDR1" flag. If set, the AMBE-frames also contain a "DSTAR stream header1"
		// header which is not used here

		if (ntohl(dstkhead_in->type) & TYPEMASK_FLG_AMB_STRHDR1) {
			this_dv_header = (struct dstar_dv_header *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct dstar_str_header1));
		} else {
			this_dv_header = (struct dstar_dv_header *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str));
		}; // end else - if

		// did we expect this?
		// status = 1, stream is ongoing
		if (activestatus < 1) {
			if (pglobal->verboselevel >= 1) {
				//fprintf(stderr,"Error: DV voice-frame received when DV header expected\n");
				fprintf(stderr,"D");
			}; // end if
			continue;
		}; // end if


		// is it the correct streamid?
		if (this_dv_header->dv_stream_id != activestreamid) {
			if (pglobal->verboselevel >= 1) {
				fprintf(stderr,"Error: Stream-id mismatch: got %X expected %X. Ignoring\n",this_dv_header->dv_stream_id,activestreamid);
			}; // end if
			continue;
		}; // end if


		// is it the correct direction?
		if (direction != activedirection) {
			if (pglobal->verboselevel >= 1) {
				fprintf(stderr,"Warning: Receiving stream in opposite direction. Continuing\n");
			}; // end if
		}; // end if

		// activestatus:	1 after receiving 1st frame of stream (cfg-frame)
		// 					2 after receiving next frames (DV-frames)
		if (activestatus == 1) {
			thisfirst=1;
			activestatus=2;
		} else {
			thisfirst=0;
		}; // end else - if


		// go to DV data
		// check "STRHDR1" flag. If set, the AMBE-frames also contain a "DSTAR stream header1"
		// header which is not used here

		if (ntohl(dstkhead_in->type) & TYPEMASK_FLG_AMB_STRHDR1) {
			this_dv_data = (struct dstar_dv_data *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct dstar_dv_header) + sizeof(struct dstar_str_header1) );
		} else {
			this_dv_data = (struct dstar_dv_data *) (receivebuffer + dstkheaderoffset + sizeof(dstkheader_str) + sizeof(struct dstar_dv_header));
		}; // end else - if

		// check sequence number
		this_sequence=(uint8_t) this_dv_data->dv_seqnr & 0x1f;

		// error check
		if (this_sequence > 20) {
			if (pglobal->verboselevel >= 1) {
				fprintf(stderr,"Error: Invalid sequence number received: %d\n",this_sequence);
			}; // end if
			continue;
		}; // end if

		// last frame? (6th bit of sequence is 1)
		if ((this_dv_data->dv_seqnr & 0x40) == 0x40) {
			thislast=1;
		} else {
			thislast=0;
		};

		missed=0;
#if DEBUG > 0
fprintf(stderr,"this_sequence = %d, activesequence = %d \n",this_sequence, activesequence);
#endif

		// if needed, repeat up to 3 previous frames, after that, add empty frames added
		while (activesequence != this_sequence) {
			if (missed < 3) {
				// repeat previous voice-frame
				fprintf(stderr,"MR ");
				
				// push to in buffer: type PCM LOSS REPEAT
				ret=push_to_inbuffer(ambe_last, origin, TYPE_PCM_LOSREP, activestreamid, direction, thisfirst, thislast, NULL, pglobal);
			} else {
				// after repeating the previous voice-frame three times, we push an
				// silent frames for every missing frame
				fprintf(stderr,"MS ");
				// push to in buffer: type PCM LOSS SILENCE
				ret=push_to_inbuffer(ambe_configframe, origin, TYPE_PCM_LOSSIL, activestreamid, direction, thisfirst, thislast, NULL, pglobal);
			}; // end else - if
			missed++;

			if (missed > 30) {
				fprintf(stderr,"Problem: more then 30 frames missing. Something very strange has happened!\n");
			}; // end if

			// move active sequence up by one:
			activesequence++;
			if (activesequence > 20) {
				activesequence=0;
			}; // end if				

		}; // end while

		// push data to buffer
		fprintf(stderr,".");
		// push to in buffer: type PCM NORMAL 
		ret=push_to_inbuffer(this_dv_data->dv_voice, origin, TYPE_PCM_PCM, activestreamid, direction, thisfirst, thislast, NULL, pglobal);

//unsigned char * dvp=this_dv_data->dv_voice;
//fprintf(stderr,"P %02X %02X %02X %02X %02X %02X %02X %02X %02X \n",dvp[0],dvp[1],dvp[2],dvp[3],dvp[4],dvp[5],dvp[6],dvp[7],dvp[8]);

		// copy ambe-data (for retrievel if data is missing)
		memcpy(ambe_last,this_dv_data->dv_voice,9);

		// reset status to 0 if last frame 
		if (thislast) {
			fprintf(stderr,"ES %04X\n",activestreamid);
			activestatus=0;
		};

		// set rcvframe_valid to 1
		pglobal->rcvframe_valid=1;

		// move active sequence up by one:
		activesequence++;
		if (activesequence > 20) {
			activesequence=0;
		}; // end if				

	}; // end else - if

}; // end while (forever)

// outside "forever" loop: we should never get here unless something went wrong

pglobal->welive_mccapture=0;
return(NULL);

};

int push_to_inbuffer ( unsigned char * data, uint16_t origin, uint32_t type,  uint16_t streamid, int direction, int first, int last, char * other, globaldatastr * pglobal) {
	int buffernext;
	buffernext = (pglobal->inbuffer_high)+1;
	if (buffernext >= INBUFFERSIZE) {
		buffernext=0;
	}; // end if

	if (buffernext != pglobal->inbuffer_low) {
		// temp. var
		static ambe_bufferframe_str *p;

		p=pglobal->inbuffer[buffernext];

		// copy data

		memcpy(&p->dvdata,data,9);
		p->origin=origin; p->streamid=streamid; p->direction=direction; p->first=first;
		p->last=last; p->type=type; p->other=other;


	} else {
		if (pglobal->verboselevel >= 1) {
			fprintf(stderr,"Warning: could not store ambe-data in inbuffer!\n");
		}; // end if
		return(-1);
	}; // end if

	pglobal->inbuffer_high=buffernext;

	// re-arm "timeout" value
	pglobal->inbound_timeout=50;
	return(0);
}; // end function push_to_inbuffer
