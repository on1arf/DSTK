// xrf2amb: link to a dextra X-reflector and re-broadcast the received
// AMBE-streams to a DSTK AMBE stream


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

// defines
#define MAXSTREAMACTIVE 20



// global data: used to communicate with the dextra heardbeat function
typedef struct {
	int outsock;
	char mycall[6];
	struct sockaddr_in6 * ai_addr;
	int destport;
	int verboselevel;

	int stream_active[MAXSTREAMACTIVE];
	// 0: memory position not in use, 1: in use on a module to which
	// we are subscribed, -1: in use on a module to which we are
	// NOT subscribed
	int stream_timeout[MAXSTREAMACTIVE];

} globaldata_str;

globaldata_str global;
