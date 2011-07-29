// dstk.h //

// dstk.h: structures and defines for the DSTAR switching-matric Toolkit


// copyright (C) 2010 Kristoff Bonne ON1ARF
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
// 19 may 2011: version 0.0.1. Initial release

// global DEFINEs
#define ETHERNETMTU 1500

#define forever 1

// datastream frame structure: header: common to all streams
// However, not all fields are used in all streams
typedef struct {
	//  version: only version 1 is currently defines
	uint8_t version; 

	// flags
	uint8_t flags; // see defined below

	// origin: 16 bits that indicate where the stream has originated
	// see defines below
	uint16_t origin; 

	// DTAK data-type: see defines below
	uint32_t type;

	// sequence numbers: split up in two parts of 16 bits each
	// can be used purely sequencial or contain timeing-information
	// 'this to allow syncronisation between different streams of
	// related content (e.g. DV-stream and DTMF-stream)
	uint16_t seq1;
	uint16_t seq2;

	// streamid: split up in two part of 16 bits each
	// This allows to mark streams with additional information, like
	// "parent-child" relationships between streams
	uint16_t streamid1; // streamid : part 1
	uint16_t streamid2; // streamid : part 2

	// size: size of subpacket

	uint16_t size;

	uint8_t undef53; // not defined (word 5 octet 3)
	uint8_t undef54; // not defined (word 5 octet 4)
} dstkheader_str ;


// flags:
#define DSTK_FLG_LAST 0x80 // last subframe in DSTK superframe


// Source: identifies where a stream has originated
// the DTAK source field is 16 bits, containing 2 octets
// octet 1: namespace-id
// "00": used by the original program-code of the DSTAR Audio Tookit
// "FF": used for private or experimental address space
// Octet 2 is only defined for namespace-id 0

#define ORIGINMASK_GROUP 0xfff0

// misc not belonging to a certain group
#define ORIGIN_UNDEF 0x0000  // origin undefined or not applicable
#define ORIGIN_FILLER 0x0001 // origin: frames created to fill in blanks in
                             // streams that need to be continues

// group CONFERENCES
#define ORIGIN_CNF 0x0010     // any conference origin
#define ORIGIN_CNF_DPL 0x0010 // the stream has originated by receiving streams
                              // from a dplus reflector
#define ORIGIN_CNF_DEX 0x0011 // the stream has originated by receiving streams
                              // from a dextra reflector
#define ORIGIN_CNF_STN 0x0012 // the stream has originated by receiving streams
                              // from STARnet server
#define ORIGIN_CNF_OTH 0x001F // the stream has originated by receiving streams
                              // from any other type of conferencing-service

// group locally generated
#define ORIGIN_LOC 0x0020 // any local origin
#define ORIGIN_LOC_RPC 0x0020	// the stream has originated by intercepting
                              // the traffic from/to the repeater RPC
#define ORIGIN_LOC_FILE 0x0021 // the stream has originated locally on the
                               // repeater/server from a fixed file or files
#define ORIGIN_LOC_DYN 0x0022 // the stream has originated locally on the
                              // repeater/server by dynamical process
#define ORIGIN_LOC_ANL 0x0023 // the stream has originated locally on the
                              // repeater/server from an analog source
#define ORIGIN_LOC_OTH 0x002F // the stream has originated locally on the
                              // repeater/server from any source not
                              // defined above


// group external analog source
#define ORIGIN_ANL 0x0030 // any external analog origin
#define ORIGIN_ANL_RADIO 0x0031 // the stream has originated from an external
                                // analog source: radio stream (FM or other)
#define ORIGIN_ANL_ECHOL 0x0032 // the stream has originated from an external
                                // analog source: echolink
#define ORIGIN_ANL_IRLP 0x0033 // the stream has originated from an external
                               // analog source: IRLP
#define ORIGIN_ANL_EQSO 0x0034 // the stream has originated from an external
                               // analog source: e-QSO
#define ORIGIN_ANL_WIRES 0x0035 // the stream has originated from an external
                                // analog source: WIRES-QSO
#define ORIGIN_ANL_OTH 0x003F // the stream has originated from any external
                              // analog source not defined above


// group "remote system" (digital feed from a remote D-STAR system,
//                       excluding conferencing systems
#define ORIGIN_REM 0x0030 // any "remote" system
#define ORIGIN_REM_DPL 0x0031 // any "remote" system using dplus linking
#define ORIGIN_REM_DEX 0x0032 // any "remote" system using dextra linking
#define ORIGIN_REM_VOIP 0x0033 // the stream has originated from an voip-like
                               // connection from a single user-system



// TYPES of DTAK frames
// The DSTK types field is 32 bits, containing 4 octets
// octet 1: namespace-id
// "00": used by the original program-code of the DSTAR Audio Tookit
// "FF": used for private or experimental address space

// octets 2 up to 4 are only defined for namespace-id 0
// Octet 2: flags: (4 MSB bits contain flags valid for all DSTK datatypes, 4 LSB bits contain flags application to one particular group of datatypes)
// Octet 3: group of DSTK types:
// OO = "APP" streams (applications)
// 01 = "RPC" stream
// 02 = "AMBE" stream
// 03 = "PCM" stream
// Fx = datatypes application to multiple or all kinds of stream
// Octet 4: datatype

#define TYPE_APP_HTB 0x0001 // heartbeat 
#define TYPE_APP_MSG 0x0002 // application announcements
#define TYPE_APP_CMD 0x0003 // commands

#define TYPE_RPC_IP  0x0101 // Copy of IP-frame send between gateway-server
                            // and repeater-controller
#define TYPE_RPC_RPC 0x0102 // Copy of frame send between gateway-server and repeater-controller,

#define TYPE_AMB 0x0200 // GROUP: AMB-streams
#define TYPE_AMB_CFG 0x0201 // config frames
#define TYPE_AMB_DV  0x0202 // DV frames
#define TYPE_AMB_DVE 0x0203 // extended DV frames

#define TYPE_PCM     0x0300 // group: PCM frames
#define TYPE_PCM_PCM 0x0301 // normal PCM-frame containing voice
#define TYPE_PCM_FLL 0x0302 // empty PCM-frame used for stuffing when no
                            //  valid DV-frames received. Only used when the
                            // "sendalways" DEBUG flag is set
#define TYPE_PCM_LOSREP 0x0303 // repetition of previous PCM-frame to deal
                               // with packetloss

#define TYPE_PCM_LOSSIL 0x0304 // empty (silent) PCM-frame do deal with packetloss


// all kind of "other", numbered from 0xE000 upwards, no further logical in numbering
#define TYPE_OTH_DPLGWMSG 0xe000 // "gwmsg=" message coming from DPLUS gateway


// types common to multiple type of DSTK frames
// group 0: DV-frames
#define TYPE_DV_DTMF			0xf001

// types common to all types of DSTK frames
#define TYPE_ALL_ADR       0xff01 // DSTAR addressing information (my, yr, rpt1,
                                  // ... etc)
#define TYPE_ALL_ERRSTAT   0xff02 // DSTAR error statistics


// typemask flags common for ALL types of data (4 Most significant bit)

#define TYPEMASK_FLG_DIR   0x0080000 // direction: 0=inbound, 1=outbound

// "first" and "last" flag: set during the beginning or the end
// of a stream. Only used when this information cannot be retrieved
// via some other way (e.g. for PCM-streams)
#define TYPEMASK_FLG_FIRST 0x00400000 // first frame of a stream
#define TYPEMASK_FLG_LAST  0x00200000 // last frame of stream

// typemask flags only for certain kinds of data (4 least significant bits)

// flags related to AMB-stream
#define TYPEMASK_FLG_AMB_STRHDR1 0x00080000 // AMBE stream contains both
//    dstar_str_header1 and dstar_str_header2
// e.g. is the case for dextra and dplus stream. Is NOT the case for
// streams from RPC

// flags related to AMB-streams originating from a conference:
#define TYPEMASK_FLG_AMB_CNF_RPT1 0x00040000 // For AMBE-streams originating
// from a conference-system (e.g. reflector).
// If set, the callsign/module of the conference is found in rpt1 field
// If not set, the callsign/module of the conference is found in rpt2 field



// mask-filter: blank out bits used for flags
#define TYPEMASK_FILT_NOFLAGS 0xFF00FFFF
#define TYPEMASK_FILT_GRP  0xFF00FF00
// End of global data


// DSTK signature
unsigned char dstk_signature[] = {'D','S','T','K'}; 

// start data structure configuration

// data structures used for decoding RPC frames
struct dstar_rpc_header {
        char dstar_id[4];
        u_short dstar_pkt_num;
        u_char dstar_rs_flag;
        u_char dstar_pkt_type;
#define DSTAR_PKT_TYPE_DD   0x11
#define DSTAR_PKT_TYPE_DV   0x12
#define DSTAR_PKT_TYPE_MODULE_HEARD   0x21
#define DSTAR_PKT_TYPE_ACK   0x00
        u_short dstar_data_len;
};

// part 1 of internet-based D-STAR streams (callsign routed calls,
//                  Dextra, dplus)
// header of DSTAR packet part of an internetstream
struct dstar_str_header1 {
	char dstarstr_id[4];
	uint8_t dstarstr_type;
#define DSSTR_TYPE_CFG 0x10
#define DSSTR_TYPE_DV 0x20
	uint8_t dstarstr_unknown[3];
}; // end


// part 2 of internet-based D-STAR streams (callsign routed calls,
//                  Dextra, dplus)
// header of DSTAR packet part of an internetstream
// this header is also present on RPC-generated streams
// but it is known as dstar_dv_header overthere (see below)
struct dstar_str_header2 {
	uint8_t dstarstr_voicedata;
#define DSSTR_VOICEDATA_VOICE 0x20
	uint8_t dstarstr_unknown[3];
	u_short dstarstr_streamid;
}; //


// DSTAR DV_HEADER: 2nd header inside packets as received/send
// between the RPC and the gateway-server
// this structure is equivalent to the dstar_str_header2 structure
// (defined above) used by streams received/send over the internet
struct dstar_dv_header {
        u_char dv_unknown1;
        u_char dv_unknown2;
        u_char dv_unknown3;
        u_char dv_module;
        u_short dv_stream_id;
};

struct dstar_dv_data {
        unsigned char dv_seqnr;
        u_char dv_voice[9];
        u_char dv_slowdata[3];
};

struct dstar_dv_rf_header {
        unsigned char dv_seqnr;
        u_char flags[3];
        char rpt2_callsign[8];
        char rpt1_callsign[8];
        char your_callsign[8];
        char my_callsign[8];
        char my_callsign_ext[4];
        u_char checksum[2];
};


struct dplus_size_header {
	// dplus size header: 2 octets, i386-order, top3 bits contain flags
	uint16_t size;
}; // end 

struct dplus_control_item {
	// commands and replies of the dplus protocol use a control-item
	// structure simular to the one used when communication with the
	// DVdongle.

	// this structure is not used when transporting DV frames
	uint16_t controlitem;
}; // end 

// end data structure configuration

