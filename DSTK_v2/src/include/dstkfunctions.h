// dstkfunctions.h //

// Functions for D-STAR switch-matrix toolskit

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

// release info:
// 13 aug. 2012: version 0.1.1. Inital release

// Function dssextract
// extract DSTK subframe from DSTK superframe

// Input:
// - pointer to DSTK header at beginning of DSTK superframe
// - total length of datastructure (for error control)
// - TYPE and TYPEMASK filter of requested dataelement
// - pointer to datastructure to return other data elements
// The main application needs to make sure it has allocated sufficient memory

// Return:
// In case of success,
// -> return value: offset of first instance that matches filter in structure (>= 0)
// -> "other" points to DSTK superframe containing all data elements not matching type/filter
// only if "other" did not point to null to start with)

// In case of not found:
// -> return value: negative value (DSSEXTR_NOTFOUND)

// In case of error
// -> return value: negative value


// error messages
#define DSSEXTRACT_NOTFOUND -1
#define DSSEXTRACT_BUFFTOSMALL -2
#define DSSEXTRACT_UNKNWNVERSION -3
#define DSSEXTRACT_OVERFLOW -4
#define DSSEXTRACT_SYNC_BUFFTOSMALL -5
#define DSSEXTRACT_SYNC_SIGNNOTFOUND -6

int dssextract (void * begin, int length, uint32_t type, uint32_t mask, void * other, int * othersize) {

// we start seting foundit to the error code "NOT FOUND"
int foundit=DSSEXTRACT_NOTFOUND;
int dstkheaderoffset=0;

int otheroffset=0;
int lastotheroffset=0;

int subframesize;

dstkheader_str * dstkhead_in;

// sanitycheck:
// check size: should be at least the size of the header
if (length < sizeof(dstkheader_str)) {
	return(DSSEXTRACT_BUFFTOSMALL);
}; // end if


while (forever) {
	// check DSTK packet header
	dstkhead_in = (dstkheader_str *) (begin + dstkheaderoffset);

	if (dstkhead_in->version != 1) {
		return(DSSEXTRACT_UNKNWNVERSION);
	}; // end if

	subframesize=ntohs(dstkhead_in->size);

	if ((ntohl(dstkhead_in->type) & mask) == type) {
		// OK, found type of data we where looking for
		foundit=dstkheaderoffset;

	} else {
		// found something but it not what we where looking for
		// copy it to the "other" field

		// note: the "other" field must point to an area of memory which has been to be
		// alloced by malloc before by the main application
		
		// ignore this is other points to NULL
		if (other != NULL) {
			dstkheader_str * lastdsshead;

			// sanity check:
			// this subframe must be completely inside superframe
			if (dstkheaderoffset + subframesize > length) {
				return(DSSEXTRACT_OVERFLOW);
			}; // end if

			// things to do when not the first DSS subframe of DSTK superframe
			if (otheroffset > 0) {
				// change "last" flag in last subframe to "no"
				lastdsshead = (dstkheader_str *) other + lastotheroffset;
				lastdsshead->flags &= (0xFF ^ DSTK_FLG_LAST);

				// add DSTK-signature
				memcpy(other+otheroffset,"DSTK",4);
				otheroffset += 4;
			}; // end if
			
			// copy data to "other" memory, including DSTK subframe header
			memcpy(other+otheroffset,dstkhead_in,sizeof(dstkheader_str)+subframesize);


			// before moving up the otheroffset pointer, set the "last" bit in the
			// the currently last subframe to 1
			lastdsshead = (dstkheader_str *) other + otheroffset;
			lastdsshead->flags |= DSTK_FLG_LAST;

			// copy otheroffset to lastotheroffset, needed when adding next subframe
			lastotheroffset = otheroffset;

			otheroffset += sizeof(dstkheader_str) + subframesize;
		}; // end if

	}; // end if

	if (dstkhead_in->flags | DSTK_FLG_LAST) {
		// stop if last frame-market set
		break;
	}; // end if


	// sanity check
	// is there place anyway?
	if (dstkheaderoffset + subframesize + sizeof(dstk_signature) + sizeof(dstkheader_str) > length) {
		// nope.
		// this looks like a syncronisation error: break out completely
		return(DSSEXTRACT_SYNC_BUFFTOSMALL);
	}; // end if

	// move up pointer
	dstkheaderoffset+=sizeof(dstkheader_str) + subframesize;

	// sanity check
	// check for signature

	// check for signature 'DSTK'
	if (memcmp(begin+dstkheaderoffset,dstk_signature,sizeof(dstk_signature))) {
		// signature not found: syncronisation error: break out completely
		return(DSSEXTRACT_SYNC_SIGNNOTFOUND);
	}; // end if

	// OK, all is OK. move up pointer
	dstkheaderoffset += sizeof(dstk_signature);

	// end of loop: next subframe
}; // end while


// copy size of "other"
*othersize=otheroffset;

// return with pointer to position

return(foundit);

}; // end function DSS extract

// dssextract_printerr: return string with textual description of error
const char * dssextract_printerr (int errcode) {

if (errcode >= 0) {
	return ("NO ERROR");
} else if (errcode == DSSEXTRACT_NOTFOUND) {
	return ("NOT FOUND");
} else if (errcode == DSSEXTRACT_BUFFTOSMALL) {
	return ("Buffer not large enought to contain DSTK subframe header");
} else if (errcode == DSSEXTRACT_UNKNWNVERSION) {
	return ("DSTK header version 1 expected.");
} else if (errcode == DSSEXTRACT_OVERFLOW) {
	return ("subframe overflow: length-indication points behind end of frame");
} else if (errcode == DSSEXTRACT_SYNC_BUFFTOSMALL) {
	return ("subframe syncronisation error: frame not large enough to hold signature + next subframe");
} else if (errcode == DSSEXTRACT_SYNC_SIGNNOTFOUND) {
	return("subframe syncronisation error: DSTK signature missing before next subframe");
} else {
	return ("UNKOWN ERROR");
}; // end else - elsif (...) - if

}; // end function
