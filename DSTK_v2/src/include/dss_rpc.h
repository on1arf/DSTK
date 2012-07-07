// Dstk SubStream RPC structure  //

// structure begins here
#ifndef DSS_RPC
#define DSS_RPC


// include IP and UDP header files if not yet done
#ifndef __NETINET_IP_H
#include <netinet/ip.h>
#endif

#ifndef __NETINET_UDP_H
#include <netinet/udp.h>
#endif


// RPC header
struct dstar_rpc_header {
        unsigned char dstar_id[4];
        uint16_t dstar_pkt_num;
        uint8_t dstar_rs_flag;
        uint8_t dstar_pkt_type;
#define DSTAR_PKT_TYPE_DD   0x11
#define DSTAR_PKT_TYPE_DV   0x12
#define DSTAR_PKT_TYPE_MODULE_HEARD   0x21
#define DSTAR_PKT_TYPE_ACK   0x00
        uint16_t dstar_data_len;
};


// a RPC header SubStructream structure:
// IP header + UDP header + RPC header
struct dss_rpc_header {
	struct iphdr ipheader;
	struct udphdr udpheader;
	struct dstar_rpc_header rpcheader;
}; // end structure

#endif
