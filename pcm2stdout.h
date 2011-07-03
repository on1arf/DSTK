/* pcm2stdout.h */

// pcm2stdout: join PCM-stream and send to standard out, optionally
// adding silence or noise when no voice is being received

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
#define PCMBUFFERSIZE 320

#define MULTICASTBUFFSIZE 65536
#define ETHERNETMTU 1500

#define forever 1

// start data structure configuration

typedef struct {
	uint32_t version; // currently only one version: 1
	uint32_t type; // 0 (silent PCM), 1 (valid PCM)
	unsigned char pcm[PCMBUFFERSIZE];
} mcframe_pcm_str;

// end data structure configuration

// global data
typedef struct {
	// dual-buffer approach
	unsigned char * buffer[2];
	int activebuffer;
	int datatosend;

	// config data
	int verboselevel;
	int addsilence;
	int addnoise;
} globaldatastr;

globaldatastr global;


