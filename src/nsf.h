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
 */
typedef uint8_t nsf_chipExtensions;
#define NSF_CHIP_VRCVI         1
#define NSF_CHIP_VRCVII        2
#define NSF_CHIP_FDS           4
#define NSF_CHIP_MMC5          8
#define NSF_CHIP_NAMCO106      16
#define NSF_CHIP_SUNSOFT_FME07 32

/**
 * The header for the NESM format
 * This structure must have the size 0x80 bytes (sizeof(struct nsf_nesmHeader)=0x80) with no padding anywhere.
 * Unfortunately there's no static assert at the moment in C for controlling this.
 * All integers in this structure have little endian byte order
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
	char               gameTitle[32];//Not neccesarily null-terminated
	char               artist[32];   //Not neccesarily null-terminated
	char               copyright[32];//Not neccesarily null-terminated
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
	struct nsfe_infoChunk info;

	//Old NESM speed stuff
	int ntscPlaySpeed;
	int palPlaySpeed;

	//Song/note data
	uint8_t* dataBuffer;  //the buffer containing NSF code.  If loadData was false when loading the NSF, this is NULL
	size_t dataBufferSize;//the size of the above buffer.  0 if loadData was false

	//Playlist
	uint8_t* playlist;  //Contains the playlist (NULL if none exists).  Each entry is the zero based index of the song to play
	size_t playlistSize;//Size of the playlist, the number of tracks

	//Track informations (Fixed size lists)
	//NULL if no track times specified, otherwise these MUST BE (trackCount*sizeof(*<variable>)) in size.
	int32_t* trackTimes; //Contains track times.
	int32_t* trackFades; //Contains track fade times.
	char** trackLabels;//Contains track label strings that are null terminated.

	//File information (null terminated strings or NULL if not specified)
	char* gameTitle;//Name of the game.
	char* artist;   //Artist of the music.
	char* copyright;//Copyright information.
	char* ripper;   //Name of the one who ripped the NSF.

	//Bankswitching info
	uint8_t bankSwitch[8];//The initial bankswitching registers needed for some NSFs.  If the NSF does not use bankswitching, these values will all be zero
};

/**
 * Return codes for the nsf_load functions.
 */
enum nsf_load_return{
	NSFLOAD_SUCCESS                    =  0,
	NSFLOAD_ALLOCATION_ERROR           = -1,
	NSFLOAD_WRONG_HEADER_TYPE          = -2,
	NSFLOAD_WRONG_HEADER_TYPE2         = -3,
	NSFLOAD_WRONG_HEADER_VERSION       = -4,

	NSFLOAD_MULTIPLE_SINGLE_CHUNKS     = -101,
	NSFLOAD_TOO_SMALL_CHUNK            = -102,
	NSFLOAD_TOO_BIG_CHUNK              = -103,
	NSFLOAD_REQ_CHUNK_NOT_DEFINED_YET  = -104,
	NSFLOAD_UNKNOWN_REQ_CHUNK          = -105,
	NSFLOAD_EOF_WITHOUT_NENDCHUNK      = -106,
};

/**
 * Return codes for the nsf_save functions.
 */
enum nsf_save_return{
	NSFSAVE_SAVE                    =  0,
	NSFSAVE_ALLOCATION_ERROR        = -1,
};

/**
 * Load a file to the nsf_data structure.
 * If loadData is false, the NSF code is not loaded, only the other information (like track times, game title, Author, etc).
 * If you're loading an NSF with intention to play it, loadData must be true.
 * This function will not assume that the nsf_data structure is filled with zeroes.
 * The resulted nsf_data structure MUST ALWAYS be freed using nsf_free.
 *
 * @param nsf      Output, structure to write to.
 *                 Must be non-null and a valid nsf_data structure.
 * @param file     Input, file to read from
 *                 Must be non-null and a valid FILE.
 * @param loadData Whether to load the song data
 */
enum nsf_load_return nsf_load(struct nsf_data* nsf,FILE* file,bool loadData);
enum nsf_load_return nsf_loadNesm(struct nsf_data* nsf,FILE* file,bool loadData);
enum nsf_load_return nsf_loadNsfe(struct nsf_data* nsf,FILE* file,bool loadData);

/**
 * Save the nsf_data to a file.
 *
 * @param nsf      Input, structure to read from.
 *                 Must be non-null and a valid nsf_data structure.
 * @param file     Output, file to write to.
 *                 Must be non-null and a valid FILE.
 */
enum nsf_save_return nsf_saveNesm(const struct nsf_data* nsf,FILE* file);
enum nsf_save_return nsf_saveNsfe(const struct nsf_data* nsf,FILE* file);

/**
 * Initiates the nsf_data structure.
 * This should be called before loading.
 *
 * @param nsf The nsf_data structure to initialize.
 */
void nsf_init(struct nsf_data* nsf);

/**
 * Cleans up memory that the load functions allocated.
 * One nsf_free for every nsf_load function called.
 *
 * @param nsf The nsf_data structure to free.
 *            Must be non-null and a valid nsf_data structure.
 */
void nsf_free(const struct nsf_data* nsf);

#endif
