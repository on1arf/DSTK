/* amb2pcm.h */

// amb2pcm: capture DV-stream, decode and stream to multicast

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

//  release info:
// 14 jun. 2011: version 0.0.1. Initial release

// global DEFINEs
#define INBUFFERSIZE 128
#define OUTBUFFERSIZE 128

#define PCMBUFFERSIZE 320

#define MULTICASTBUFFSIZE 65536
#define ETHERNETMTU 1500

#define forever 1

// packets sent to dongle:
// starts with two bytes containing length of packet and packet type

// command to dongle: request name
const unsigned char request_name[] = {0x04, 0x20, 0x01, 0x00};
// command to dongle: start
const unsigned char command_start[] = {0x05, 0x00, 0x18, 0x00, 0x01};
// command to dongle: stop
const unsigned char command_stop[] = {0x05, 0x00, 0x18, 0x00, 0x02};

// ambe-packet: does not contain any actual ambe-data (last 24 bytes) but
// contains configuration-setting for ambe-encoder
// see http://www.moetronix.com/dvdongle and http://www.dvsinc.com/products/a2020.htm for more info
unsigned char ambe_configframe[] = {0x32, 0xA0,0xEC, 0x13, 0x00, 0x00, 0x30, 0x10, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x04, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ambe valid-frame: contains the same data as the config-frame to start
// with but the first 9 octets of the actual ambe-data will be overwritten
// by the actual ambe-data that is received
unsigned char ambe_validframe[] = {0x32, 0xA0,0xEC, 0x13, 0x00, 0x00, 0x30, 0x10, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x04, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};



// ambe_buffer_dataframe: used as buffer to transfer data from in to out
typedef struct {
	unsigned char dvdata[9]; // AMBE data
	uint16_t origin; // origin: copied from in-packet to out-packet
	uint16_t streamid; // sequence: in.streamid1 copied to out.streamid2
	int direction; // flag direction
	int first; // flag first
	int last; // flag last
	uint32_t type; // TYPE_PCM_PCM, TYPE_PCM_LOSREP or TYPE_PCM_SIL
	char * other; // other dstk subframes that have to copied over
} ambe_bufferframe_str;

ambe_bufferframe_str ambe_bufferframe;

// End of global data



// start data structure configuration

// structure of PCM-frame send and received from/to dongle
struct {
	char buffsize1;
	char buffsize2;
	char pcm[PCMBUFFERSIZE];
} pcmbuffer;

struct {
	char buffsize1;
	char buffsize2;
	char pcm[PCMBUFFERSIZE];
} pcmbuffer_silent;

// multicast frame: send outbound
typedef struct {
	dstkheader_str dstkheader;
	unsigned char pcm[PCMBUFFERSIZE];
} mcframe_pcm_str;

#define PCMTYPE_NORMAL 0
#define PCMTYPE_LOSS_REPEAT 1
#define PCMTYPE_LOSS_SILENT 2
#define PCMTYPE_UNDEF 0xfffffff


// end data structure configuration

// global data-structure. This is used to communicate between threads
// The global data is initialy filled in by the main part of the process
// and then send to the child-processes as parameter during startup

// data that does not changed (like verboselevel) is passed as normal data
// data that does change (like p_pingbuffer_low and _high) is passed as pointer

typedef struct {
int donglefd;

int welive_mccapture;
int welive_serialreceive;

int rcvframe_valid;
int serframe_valid;
int sndframe_valid;

int outbuffer_low;
int outbuffer_high;

int inbuffer_low;
int inbuffer_high;

// data-structures for communication between the threads
int verboselevel;

char * s_ipaddress; char * d_ipaddress;
int s_port; int d_port;
char module;
ambe_bufferframe_str * inbuffer[INBUFFERSIZE];
ambe_bufferframe_str lastambeframe;

mcframe_pcm_str * outbuffer[OUTBUFFERSIZE];


int inbound_timeout;

int repeatermodactive[3];
int reflectormodactive[3];
int allmodule;

uint16_t globalstreamid;

} globaldatastr; // global data structure

globaldatastr global;
