// rpc2amb.h //

// rpc2amb: join 'RPCIP'-stream, and multicast out as one 'AMB'-stream
// per module


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
#define ETHERNETMTU 1500

// data structure have been moved to datk.h

// data-structure "global"
typedef struct {
	int inbound_timeout[3];
	int activestatus[3];
} global_str;

global_str global;

