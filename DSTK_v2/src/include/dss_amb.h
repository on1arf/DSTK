// Dstk SubStream AMB structure  //

// structure begins here
#ifndef DSS_AMB
#define DSS_AMB


// part 1 of internet-based D-STAR streams (callsign routed calls,
//                  Dextra, dplus)
// header of DSTAR packet part of an internetstream
struct dstar_header1 {
        char signature[4];
        uint8_t type;
#define DSSTR_TYPE_RTA 0x10
#define DSSTR_TYPE_DV 0x20
        uint8_t unknown5;
        uint8_t unknown6;
        uint8_t unknown7;
}; // end


// part 2 of internet-based D-STAR streams (callsign routed calls,
//                  Dextra, dplus)
// header of DSTAR packet part of an internetstream
// this header is also present as the 2nd header inside RPC-generated
// streams 
// but it is known as dstar_dv_header overthere (see below)
struct dstar_header2 {
        uint8_t voiceordata;
#define DSSTR_VOICEORDATA_VOICE 0x20
        uint8_t unknown1;
        uint8_t unknown2;
        uint8_t rpc_module; // used for traffic originating
									 // from a RPC
        uint16_t streamid;
        uint8_t dv_seqnr;
}; //

struct dstar_dv_data {
        unsigned char dv_voice[9];
        unsigned char dv_slowdata[3];
};

struct dstar_dv_artheader {
        unsigned char flags[3];
        unsigned char rpt2_callsign[8];
        unsigned char rpt1_callsign[8];
        unsigned char your_callsign[8];
        unsigned char my_callsign[8];
        unsigned char my_callsign_ext[4];
        unsigned char checksum[2];
};

struct dstar_fullframe_dvdata {
        struct dstar_header1 dstarheader1;
        struct dstar_header2 dstarheader2;
        struct dstar_dv_data dvdata;
}; 

struct dstar_fullframe_dvdataext {
        struct dstar_header1 dstarheader1;
        struct dstar_header2 dstarheader2;
        struct dstar_dv_data dvdata;
        unsigned char ext[3];
}; 

struct dstar_fullframe_art {
        struct dstar_header1 dstarheader1;
        struct dstar_header2 dstarheader2;
        struct dstar_dv_artheader dvartheader;
}; 

// any possible DSTAR DV-frame
// union structure
typedef union {
        struct dstar_fullframe_dvdata dvdata;
        struct dstar_fullframe_dvdataext dvdataext;
        struct dstar_fullframe_art art;
} dstar_full_u; 

// fullframe definition
dstar_full_u dss_ambe;

#endif
