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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <endian.h>
#include "nsf.h"
#include "nsf_region.h"

#ifndef MIN
	#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

enum nsf_load_return nsf_loadNesm(struct nsf_data* nsf,FILE* file,bool loadData){
	//Read the header (Copy the whole header to the structure)
	struct nsf_nesmHeader header;
	fread(&header,sizeof(struct nsf_nesmHeader),1,file);

	//Confirm the header type for NESM
	if(memcmp(header.type,NSF_HEADERTYPE_NESM,NSF_HEADERTYPE_LENGTH)!=0)
		return NSFLOAD_WRONG_HEADER_TYPE;
	if(header.typeExtra!=0x1A)
		return NSFLOAD_WRONG_HEADER_TYPE2;
	if(header.version!=1)//TODO: Some badly formatted NSFs claims to be above version 1
		return NSFLOAD_WRONG_HEADER_VERSION;

	//Copy data from nsf_nesmHeader to nsf_data
	nsf->info.region         = header.region & 0x03;
	nsf->palPlaySpeed        = header.speedPAL; //blarg
	nsf->ntscPlaySpeed       = header.speedNTSC;//blarg
	nsf->info.loadAddress    = le16toh(header.loadAddress);
	nsf->info.initAddress    = le16toh(header.initAddress);
	nsf->info.playAddress    = le16toh(header.playAddress);
	nsf->info.chipExtensions = header.chipExtensions;

	nsf->info.trackCount   = header.trackCount;    //0-based number copied
	nsf->info.initialTrack = header.initialTrack-1;//1-based number from header converted to 0-based

	memcpy(nsf->bankSwitch,header.bankSwitch,sizeof(header.bankSwitch));

	//Allocate space for strings and +1 for null termination
	if(!(nsf->gameTitle = malloc(sizeof(header.gameTitle) + sizeof(char)*1))
	|| !(nsf->artist    = malloc(sizeof(header.artist)    + sizeof(char)*1))
	|| !(nsf->copyright = malloc(sizeof(header.copyright) + sizeof(char)*1)))
		return NSFLOAD_ALLOCATION_ERROR;

	//Copy strings and append a null terminator in case the string is filling the field
	memcpy(nsf->gameTitle,header.gameTitle,sizeof(header.gameTitle));
	nsf->gameTitle[sizeof(header.gameTitle)] = '\0';

	memcpy(nsf->artist   ,header.artist   ,sizeof(header.artist));
	nsf->artist   [sizeof(header.artist)]    = '\0';

	memcpy(nsf->copyright,header.copyright,sizeof(header.copyright));
	nsf->copyright[sizeof(header.copyright)] = '\0';

	//Read the NSF data
	if(loadData){
		//Determine the size of the data
		fseek(file,0,SEEK_END);
		int dataSize = ftell(file)-sizeof(struct nsf_nesmHeader);
		fseek(file,sizeof(struct nsf_nesmHeader),SEEK_SET);

		//Allocate space for data
		if(!(nsf->dataBuffer = malloc(sizeof(uint8_t)*dataSize)))
			return NSFLOAD_ALLOCATION_ERROR;

		//Read data and set data size
		fread(nsf->dataBuffer,dataSize,1,file);
		nsf->dataBufferSize = dataSize;
	}

	//If the function have reached this point, then it was a successful read
	return NSFLOAD_SUCCESS;
}

enum nsf_save_return nsf_saveNesm(const struct nsf_data* nsf,FILE* file){
	//Initialize header data and simply copying from the nsf
	struct nsf_nesmHeader header = {
		.type           = NSF_HEADERTYPE_NESM,
		.typeExtra      = 0x1A,
		.version        = 1,
		.trackCount     = nsf->info.trackCount,
		.initialTrack   = nsf->info.initialTrack + 1,
		.loadAddress    = htole16(nsf->info.loadAddress),
		.initAddress    = htole16(nsf->info.initAddress),
		.playAddress    = htole16(nsf->info.playAddress),
		.speedNTSC      = htole16(nsf->ntscPlaySpeed),
		.speedPAL       = htole16(nsf->palPlaySpeed),
		.region         = nsf->info.region,
		.chipExtensions = nsf->info.chipExtensions
	};

	//Copy strings and arrays of data
	if(nsf->gameTitle)
		memcpy(header.gameTitle,nsf->gameTitle,MIN(strlen(nsf->gameTitle),sizeof(header.gameTitle)));
	else
		memset(header.gameTitle,'\0',sizeof(header.gameTitle));

	if(nsf->artist)
		memcpy(header.artist,nsf->artist,MIN(strlen(nsf->artist),sizeof(header.artist)));
	else
		memset(header.artist,'\0',sizeof(header.artist));

	if(nsf->copyright)
		memcpy(header.copyright,nsf->copyright,MIN(strlen(nsf->copyright),sizeof(header.copyright)));
	else
		memset(header.copyright,'\0',sizeof(header.copyright));

	if(nsf->bankSwitch)
		memcpy(header.bankSwitch,nsf->bankSwitch,sizeof(header.bankSwitch));
	else
		memset(header.bankSwitch,'\0',sizeof(header.bankSwitch));

	//Copy header to file
	fwrite(&header,sizeof(struct nsf_nesmHeader),1,file);

	//Write data
	if(nsf->dataBufferSize>0 && nsf->dataBuffer)
		fwrite(nsf->dataBuffer,nsf->dataBufferSize,1,file);

	return NSFSAVE_SUCCESS;
}
