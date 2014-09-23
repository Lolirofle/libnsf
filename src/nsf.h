/* 
 * Copyright (C) 2004      Disch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * -------------------------------------------------------------------------
 * Code are based of the Winamp plugin "NotSoFatso v851".
 * Modified and rewritten to C by Lolirofle.
 */

#ifndef __LOLIROFLE_LIBNSF_NSF_H_INCLUDED__
#define __LOLIROFLE_LIBNSF_NSF_H_INCLUDED__

#include <stdint.h>
#include <stdbool.h>
#include "nsf_region.h"

#define NSF_HEADERTYPE_LENGTH 4
#define NSF_HEADERTYPE_NESM "NESM"
#define NSF_HEADERTYPE_NSFE "NSFE"

typedef char nsfe_chunkType;
#define NSFE_CHUNKTYPE_LENGTH 4
#define NSFE_CHUNKTYPE_INFO  "INFO"
#define NSFE_CHUNKTYPE_DATA  "DATA"
#define NSFE_CHUNKTYPE_NEND  "NEND"
#define NSFE_CHUNKTYPE_PLST  "plst"
#define NSFE_CHUNKTYPE_TIME  "time"
#define NSFE_CHUNKTYPE_FADE  "fade"
#define NSFE_CHUNKTYPE_TLBL  "tlbl"
#define NSFE_CHUNKTYPE_AUTH  "auth"
#define NSFE_CHUNKTYPE_BANK  "BANK"

/**
 * The chip extensions field in the NESM header
 * This structure must have the size 1 byte (sizeof(nsf_chipExtensions)=1) with no padding anywhere.
 * Unfortunately there's no static assert at the moment in C for controlling this.
 */
typedef struct __attribute__ ((__packed__)) nsf_chipExtensions{
	uint8_t VRCVI         :1;
	uint8_t VRCVII        :1;
	uint8_t FDS           :1;
	uint8_t MMC5          :1;
	uint8_t NAMCO106      :1;
	uint8_t SUNSOFT_FME07 :1;
}nsf_chipExtensions;

/**
 * The header for the NESM format
 * This structure must have the size 0x80 bytes (sizeof(struct nsf_nesmHeader)=0x80) with no padding anywhere.
 * Unfortunately there's no static assert at the moment in C for controlling this.
 */
struct __attribute__ ((__packed__)) nsf_nesmHeader{
	char               type[NSF_HEADERTYPE_LENGTH];
	uint8_t            typeExtra;
	uint8_t            version;
	uint8_t            trackCount;
	uint8_t            initialTrack;
	uint16_t           loadAddress;
	uint16_t           initAddress;
	uint16_t           playAddress;
	char               gameTitle[32];
	char               artist[32];
	char               copyright[32];
	uint16_t           speedNTSC;
	uint8_t            bankSwitch[8];
	uint16_t           speedPAL;
	nsf_region         region;
	nsf_chipExtensions chipExtensions;
	uint8_t            expansion[4];
};

struct __attribute__ ((__packed__)) nsfe_infoChunk{
	uint16_t           loadAddress;//The address to which the NSF code is loaded to
	uint16_t           initAddress;//The address of the Init routine (called at track change)
	uint16_t           playAddress;//The address of the Play routine (called several times a second)
	nsf_region         region;
	nsf_chipExtensions chipExtensions;//Bitwise representation of the external chips used by this NSF.  Read NSFSpec.txt for details

	//Track info
	uint8_t trackCount;  //The number of tracks in the NSF (1 = 1 track, 5 = 5 tracks, etc)
	uint8_t initialTrack;//The initial track (ZERO BASED:  0 = 1st track, 4 = 5th track, etc)
};

struct nsf_data{
	//basic NSF info
	struct nsfe_infoChunk;
	
	//Old NESM speed stuff (blarg)
	int ntscPlaySpeed;
	int palPlaySpeed;

	//nsf data
	uint8_t* dataBuffer;    //the buffer containing NSF code.  If loadData was false when loading the NSF, this is NULL
	int      dataBufferSize;//the size of the above buffer.  0 if loadData was false

	//Playlist
	uint8_t* playlist;    //the buffer containing the playlist (NULL if none exists).  Each entry is the zero based index of the song to play
	int      playlistSize;//the size of the above buffer (and the number of tracks in the playlist)

	//Track informations
	int*   trackTimes;//the buffer containing the track times.  NULL if no track times specified. Otherwise this buffer MUST BE (nTrackCount * sizeof(int)) in size
	int*   trackFades;//the buffer containing the track fade times.  NULL if none are specified.  Same conditions as trackTimes
	char** trackLabels;//the buffer containing track labels.  NULL if there are no labels.  There must be nTrackCount char pointers (or none if NULL).
	                   //Each pointer must point to it's own buffer containing a string (the length of this buffer doesn't matter, just so long as the string is NULL terminated)
	                   // the string's buffer may be NULL if a string isn't needed
	                   //szTrackLabels as well as all of the buffers it points to are destroyed upon
	                   // a call to Destroy (or the destructor).

	//File information (NULL-terminated strings or NULL if not specified)
	char* gameTitle;//Name of the game
	char* artist;   //Artist of the music
	char* copyright;//Copyright information
	char* ripper;   //Name of the one who ripped the NSF

	//Bankswitching info
	uint8_t bankSwitch[8];//The initial bankswitching registers needed for some NSFs.  If the NSF does not use bankswitching, these values will all be zero
};

/**
 * Loads a nsf structure from a specified path.
 * If loadData is false, the NSF code is not loaded, only the other information (like track times, game title, Author, etc).
 * If you're loading an NSF with intention to play it, loadData must be true.
 */
int nsf_load(struct nsf_data* nsf,FILE* file,bool loadData);

/**
 * Saves the NSF to a file
 */
int nsf_saveNesm(const struct nsf_data* nsf,FILE* file);
int nsf_saveNsfe(const struct nsf_data* nsf,FILE* file);

/**
 * Cleans up memory
 */
void nsf_free(const struct nsf_data* nsf);

#endif
