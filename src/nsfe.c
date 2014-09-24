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

#define SAFE_NEW(ptr,type,n,failureReturn) if(!(ptr=malloc(sizeof(type)*n)))return failureReturn;memset(ptr,'\0',sizeof(type)*n)

enum nsf_load_return nsf_loadNsfe(struct nsf_data* nsf,FILE* file,bool loadData){
	//Data for chunk iterations
	nsfe_chunkType chunkType[NSFE_CHUNKTYPE_LENGTH];
	uint32_t chunkSize;
	int chunkUsed;

	//Checks for single chunks
	bool infoFound = false,
	     bankFound = false,
	     dataFound = false;

	{//Confirm the header type for NSFE
		char headerType[NSF_HEADERTYPE_LENGTH];
		if(fread(headerType,1,NSF_HEADERTYPE_LENGTH,file)!=NSF_HEADERTYPE_LENGTH
		|| memcmp(headerType,NSF_HEADERTYPE_NSFE,NSF_HEADERTYPE_LENGTH)!=0)
			return NSFLOAD_WRONG_HEADER_TYPE;
	}

	//Begin reading chunks
	while(true){
		if(feof(file))
			return NSFLOAD_EOF_WITHOUT_NENDCHUNK;

		//Read chunk size and chunk type
		fread(&chunkSize,sizeof(chunkSize),1,file);
		chunkSize = le32toh(chunkSize);
		fread(chunkType,NSFE_CHUNKTYPE_LENGTH,1,file);

		//Determine which chunk type it is
		if(memcmp(chunkType,NSFE_CHUNKTYPE_INFO,NSFE_CHUNKTYPE_LENGTH)==0){
			//Restrict to one of this type of chunk
			if(infoFound)
				return NSFLOAD_MULTIPLE_SINGLE_CHUNKS;
			//Restrict to a minimum size of this chunk
			if(chunkSize<8)
				return NSFLOAD_TOO_SMALL_CHUNK;

			infoFound = true;

			//Default values
			nsf->info.trackCount = 1;

			chunkUsed = MIN((uint32_t)sizeof(struct nsfe_infoChunk),chunkSize);

			//Read data, fixing endian
			fread(&nsf->info,chunkUsed,1,file);
			nsf->info.loadAddress = le16toh(nsf->info.loadAddress);
			nsf->info.initAddress = le16toh(nsf->info.initAddress);
			nsf->info.playAddress = le16toh(nsf->info.playAddress);

			fseek(file,chunkSize-chunkUsed,SEEK_CUR);

			nsf->palPlaySpeed  = (uint16_t)(1000000/NSF_PAL_NMIRATE); //blarg
			nsf->ntscPlaySpeed = (uint16_t)(1000000/NSF_NTSC_NMIRATE);//blarg
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_DATA,NSFE_CHUNKTYPE_LENGTH)==0){
			//The info chunk must be defined before this chunk
			if(!infoFound)
				return NSFLOAD_REQ_CHUNK_NOT_DEFINED_YET;
			//Restrict to one data chunk
			if(dataFound)
				return NSFLOAD_MULTIPLE_SINGLE_CHUNKS;
			//Restrict to a minimum size of this chunk
			if(chunkSize<1)
				return NSFLOAD_TOO_SMALL_CHUNK;

			dataFound = true;

			//Load the data if necessary
			if(loadData){
				nsf->dataBufferSize = chunkSize;
				if(!(nsf->dataBuffer = malloc(sizeof(uint8_t)*nsf->dataBufferSize)))
					return NSFLOAD_ALLOCATION_ERROR;
				fread(nsf->dataBuffer,nsf->dataBufferSize,1,file);
			}else{
				nsf->dataBufferSize = 0;
				fseek(file,chunkSize,SEEK_CUR);
			}
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_NEND,NSFE_CHUNKTYPE_LENGTH)==0){
			break;
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_TIME,NSFE_CHUNKTYPE_LENGTH)==0){//TODO: Endian
			//The info chunk must be defined before this chunk
			if(!infoFound)
				return NSFLOAD_REQ_CHUNK_NOT_DEFINED_YET;
			//Restrict to one of this type of chunk
			if(nsf->trackTimes)
				return NSFLOAD_MULTIPLE_SINGLE_CHUNKS;

			if(!(nsf->trackTimes = calloc(nsf->info.trackCount,sizeof(nsf->trackTimes[0]))))
				return NSFLOAD_ALLOCATION_ERROR;
			chunkUsed = MIN(chunkSize/4,nsf->info.trackCount);

			fread(nsf->trackTimes,chunkUsed,4,file);
			fseek(file,chunkSize-(chunkUsed*4),SEEK_CUR);

			while(chunkUsed < nsf->info.trackCount)
				nsf->trackTimes[chunkUsed++] = -1;	//negative signals to use default time
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_FADE,NSFE_CHUNKTYPE_LENGTH)==0){//TODO: Endian
			//The info chunk must be defined before this chunk
			if(!infoFound)
				return NSFLOAD_REQ_CHUNK_NOT_DEFINED_YET;
			//Restrict to one of this type of chunk
			if(nsf->trackFades)
				return NSFLOAD_MULTIPLE_SINGLE_CHUNKS;

			if(!(nsf->trackFades = calloc(nsf->info.trackCount,sizeof(nsf->trackFades[0]))))
				return NSFLOAD_ALLOCATION_ERROR;
			chunkUsed = MIN(chunkSize / 4,nsf->info.trackCount);

			fread(nsf->trackFades,chunkUsed,4,file);
			fseek(file,chunkSize - (chunkUsed * 4),SEEK_CUR);

			for(;chunkUsed<nsf->info.trackCount;++chunkUsed)
				nsf->trackFades[chunkUsed] = -1;	//negative signals to use default time
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_BANK,NSFE_CHUNKTYPE_LENGTH)==0){
			//Restrict to one of this type of chunk
			if(bankFound)
				return NSFLOAD_MULTIPLE_SINGLE_CHUNKS;

			bankFound = true;
			chunkUsed = MIN(8,chunkSize);

			fread(nsf->bankSwitch,chunkUsed,1,file);
			fseek(file,chunkSize - chunkUsed,SEEK_CUR);
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_PLST,NSFE_CHUNKTYPE_LENGTH)==0){
			//Restrict to one of this type of chunk
			if(nsf->playlist)
				return NSFLOAD_MULTIPLE_SINGLE_CHUNKS;

			nsf->playlistSize = chunkSize;

			//If playlist data exists
			if(nsf->playlistSize>0){
				if(!(nsf->playlist = calloc(nsf->playlistSize,sizeof(nsf->playlist[0]))))
					return NSFLOAD_ALLOCATION_ERROR;
				fread(nsf->playlist,chunkSize,1,file);
			}
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_AUTH,NSFE_CHUNKTYPE_LENGTH)==0){
			//Restrict to one of this type of chunk
			if(nsf->gameTitle)
				return NSFLOAD_MULTIPLE_SINGLE_CHUNKS;

			char* buffer;
			char* ptr;
			SAFE_NEW(buffer,char,chunkSize + 4,1);

			fread(buffer,chunkSize,1,file);//TODO: Use fgets
			ptr = buffer;

			char** ar[4] = {&nsf->gameTitle,&nsf->artist,&nsf->copyright,&nsf->ripper};//TODO: ar as in array reader? Why?
			for(int i=0;i<4;++i){
				chunkUsed = strlen(ptr) + 1;
				*ar[i] = malloc(chunkUsed);
				if(!*ar[i]) { free(buffer); return NSFLOAD_ALLOCATION_ERROR; }
				memcpy(*ar[i],ptr,chunkUsed);
				ptr += chunkUsed;
			}
			free(buffer);
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_TLBL,NSFE_CHUNKTYPE_LENGTH)==0){
			//The info chunk must be defined before this chunk
			if(!infoFound)
				return NSFLOAD_REQ_CHUNK_NOT_DEFINED_YET;
			//Restrict to one of this type of chunk
			if(nsf->trackLabels)
				return NSFLOAD_MULTIPLE_SINGLE_CHUNKS;

			if(!(nsf->trackLabels = calloc(nsf->info.trackCount,sizeof(nsf->trackLabels[0]))))
				return NSFLOAD_ALLOCATION_ERROR;

			char* buffer;
			char* ptr;
			SAFE_NEW(buffer,char,chunkSize + nsf->info.trackCount,1);

			fread(buffer,chunkSize,1,file);
			ptr = buffer;

			for(int i=0;i<nsf->info.trackCount;++i){
				chunkUsed = strlen(ptr) + 1;
				nsf->trackLabels[i] = malloc(chunkUsed);
				if(!nsf->trackLabels[i]){
					free(buffer);
					return NSFLOAD_ALLOCATION_ERROR;
				}
				memcpy(nsf->trackLabels[i],ptr,chunkUsed);
				ptr += chunkUsed;
			}
			free(buffer);
		}else{//Unknown chunk
			//If it is a required chunk, stop
			if(chunkType[0]>='A' && chunkType[0]<='Z')
				return NSFLOAD_UNKNOWN_REQ_CHUNK;
			//Otherwise, skip it
			fseek(file,chunkSize,SEEK_CUR);
		}
	}

	//If we exited the while loop without a 'return', we must have hit an NEND chunk, then the file was layed out as it was expected.
	//Minimum requirements for a valid NSFE file
		//The info chunk must be defined before having finished reading all the chunks
		if(!infoFound)
			return NSFLOAD_REQ_CHUNK_NOT_DEFINED_YET;
		//The data chunk must be defined before having finished reading all the chunks
		if(!dataFound)
			return NSFLOAD_REQ_CHUNK_NOT_DEFINED_YET;

	//If the function have reached this point, then it was a successful read
	return NSFLOAD_SUCCESS;
}

enum nsf_load_return nsf_load(struct nsf_data* nsf,FILE* file,bool loadData){
	char type[NSF_HEADERTYPE_LENGTH];
	fread(&type,4,1,file);

	//Reset file read pos
	rewind(file);

	if(memcmp(type,NSF_HEADERTYPE_NESM,NSF_HEADERTYPE_LENGTH)==0)
		return nsf_loadNesm(nsf,file,loadData);
	else if(memcmp(type,NSF_HEADERTYPE_NSFE,NSF_HEADERTYPE_LENGTH)==0)
		return nsf_loadNsfe(nsf,file,loadData);
	else
		return NSFLOAD_WRONG_HEADER_TYPE;

	return NSFLOAD_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//  File saving



enum nsf_save_return nsf_saveNsfe(const struct nsf_data* nsf,FILE* file){//TODO: MAgic numbers
	uint32_t chunkSize;
	struct nsfe_infoChunk info;

	//Header type
	fwrite(NSF_HEADERTYPE_NSFE,NSF_HEADERTYPE_LENGTH,1,file);

	//Info chunk
	chunkSize = sizeof(struct nsfe_infoChunk);
	info.chipExtensions = nsf->info.chipExtensions;
	info.region         = nsf->info.region;
	info.initAddress    = htole16(nsf->info.initAddress);
	info.loadAddress    = htole16(nsf->info.loadAddress);
	info.playAddress    = htole16(nsf->info.playAddress);
	info.initialTrack   = nsf->info.initialTrack;
	info.trackCount     = nsf->info.trackCount;

	fwrite(&chunkSize,sizeof(chunkSize),1,file);
	fwrite(NSFE_CHUNKTYPE_INFO,NSFE_CHUNKTYPE_LENGTH,1,file);
	fwrite(&info,chunkSize,1,file);

	//Bankswitching chunk if needed
	for(chunkSize=0;chunkSize<8;++chunkSize){
		if(nsf->bankSwitch[chunkSize]){
			chunkSize = 8;
			fwrite(&chunkSize,sizeof(chunkSize),1,file);
			fwrite(NSFE_CHUNKTYPE_BANK,NSFE_CHUNKTYPE_LENGTH,1,file);
			fwrite(nsf->bankSwitch,chunkSize,1,file);
			break;
		}
	}

	//Time chunk if needed
	if(nsf->trackTimes){//TODO: Endian
		chunkSize = sizeof(*nsf->trackTimes)*nsf->info.trackCount;
		fwrite(&chunkSize,sizeof(chunkSize),1,file);
		fwrite(NSFE_CHUNKTYPE_TIME,NSFE_CHUNKTYPE_LENGTH,1,file);
		fwrite(nsf->trackTimes,chunkSize,1,file);
	}

	//Fade chunk if needed
	if(nsf->trackFades){//TODO: Endian
		chunkSize = sizeof(*nsf->trackFades)*nsf->info.trackCount;
		fwrite(&chunkSize,sizeof(chunkSize),1,file);
		fwrite(NSFE_CHUNKTYPE_FADE,NSFE_CHUNKTYPE_LENGTH,1,file);
		fwrite(nsf->trackFades,chunkSize,1,file);
	}

	//auth chunk
	if(nsf->gameTitle || nsf->copyright || nsf->artist || nsf->ripper){
		chunkSize = 4;
		if(nsf->gameTitle)
			chunkSize += strlen(nsf->gameTitle);
		if(nsf->artist)
			chunkSize += strlen(nsf->artist);
		if(nsf->copyright)
			chunkSize += strlen(nsf->copyright);
		if(nsf->ripper)
			chunkSize += strlen(nsf->ripper);
		fwrite(&chunkSize,sizeof(chunkSize),1,file);
		fwrite(NSFE_CHUNKTYPE_AUTH,NSFE_CHUNKTYPE_LENGTH,1,file);

		if(nsf->gameTitle)
			fwrite(nsf->gameTitle,strlen(nsf->gameTitle) + 1,1,file);
		else
			putc('\0',file);

		if(nsf->artist)
			fwrite(nsf->artist,strlen(nsf->artist) + 1,1,file);
		else
			putc('\0',file);

		if(nsf->copyright)
			fwrite(nsf->copyright,strlen(nsf->copyright) + 1,1,file);
		else
			putc('\0',file);

		if(nsf->ripper)
			fwrite(nsf->ripper,strlen(nsf->ripper) + 1,1,file);
		else
			putc('\0',file);
	}

	//plst chunk
	if(nsf->playlist){
		chunkSize = nsf->playlistSize*sizeof(*nsf->playlist);
		fwrite(&chunkSize,sizeof(chunkSize),1,file);
		fwrite(NSFE_CHUNKTYPE_PLST,NSFE_CHUNKTYPE_LENGTH,1,file);
		fwrite(nsf->playlist,chunkSize,1,file);
	}

	//tlbl chunk
	if(nsf->trackLabels){
		chunkSize = nsf->info.trackCount;

		for(int i=0;i<nsf->info.trackCount;++i)
			chunkSize += strlen(nsf->trackLabels[i]);

		fwrite(&chunkSize,sizeof(chunkSize),1,file);
		fwrite(NSFE_CHUNKTYPE_TLBL,NSFE_CHUNKTYPE_LENGTH,1,file);

		for(int i=0;i<nsf->info.trackCount;++i){
			if(nsf->trackLabels[i])
				fwrite(nsf->trackLabels[i],strlen(nsf->trackLabels[i]) + 1,1,file);
			else
				putc('\0',file);
		}
	}

	//Data
	chunkSize = nsf->dataBufferSize;
	fwrite(&chunkSize,sizeof(chunkSize),1,file);
	fwrite(NSFE_CHUNKTYPE_DATA,NSFE_CHUNKTYPE_LENGTH,1,file);
	fwrite(nsf->dataBuffer,chunkSize,1,file);

	//end chunk
	chunkSize = 0;
	fwrite(&chunkSize,sizeof(chunkSize),1,file);
	fwrite(NSFE_CHUNKTYPE_NEND,NSFE_CHUNKTYPE_LENGTH,1,file);

	return NSFSAVE_SUCCESS;
}
