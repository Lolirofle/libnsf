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
#include "nsf.h"
#include "nsf_region.h"

#ifndef min
	#define min(a,b) (((a)<(b))?(a):(b))
#endif

#define SAFE_DELETE(ptr) {free(ptr);ptr=NULL;}
#define SAFE_NEW(ptr,type,n,failureReturn) ptr=malloc(sizeof(type)*n);if(!ptr)return failureReturn;memset(ptr,'\0',sizeof(type)*n)

void NsfFile_free(struct NsfFile* nsf){
	if(nsf==NULL)
		return;

	free(nsf->dataBuffer);
	free(nsf->playlist);
	free(nsf->trackTimes);
	free(nsf->trackFades);
	if(nsf->trackLabels){
		for(int i=0;i<nsf->trackCount;++i)
			free(nsf->trackLabels[i]);
		free(nsf->trackLabels);
	}
	free(nsf->gameTitle);
	free(nsf->artist);
	free(nsf->copyright);
	free(nsf->ripper);
}

int NsfFile_loadFile_NESM(struct NsfFile* nsf,FILE* file,bool loadData,bool ignoreversion){
	fseek(file,0,SEEK_END);
	int len = ftell(file)-0x80;
	fseek(file,0,SEEK_SET);

	if(len<1)
		return -1;

	//Read the header (Copy the whole header to the structure)
	struct NesmHeader header;
	fread(&header,0x80,1,file);

	//Confirm the header type for NESM
	if(memcmp(header.type,HEADERTYPE_NESM,HEADERTYPE_LENGTH)!=0)
		return -2;
	if(header.typeExtra!=0x1A)
		return -3;
	if(!ignoreversion && header.version!=1)//Some badly formatted NSFs claims to be above version 1
		return -4;

	//NESM is generally easier to work with (but limited!)
	//Just move the data over from NesmHeader over to our member data
	nsf->isExtended     = 0;
	nsf->region         = header.region & 0x03;
	nsf->palPlaySpeed   = header.speedPAL; //blarg
	nsf->ntscPlaySpeed  = header.speedNTSC;//blarg
	nsf->loadAddress    = header.loadAddress;
	nsf->initAddress    = header.initAddress;
	nsf->playAddress    = header.playAddress;
	nsf->chipExtensions = header.chipExtensions;

	nsf->trackCount   = header.trackCount;   //0-based number
	nsf->initialTrack =header.initialTrack-1;//1-based number from header converted to 0-based

	memcpy(nsf->bankSwitch,header.bankSwitch,8);

	SAFE_NEW(nsf->gameTitle,char,33,1);
	SAFE_NEW(nsf->artist   ,char,33,1);
	SAFE_NEW(nsf->copyright,char,33,1);

	memcpy(nsf->gameTitle,header.gameTitle,32);
	memcpy(nsf->artist   ,header.artist   ,32);
	memcpy(nsf->copyright,header.copyright,32);

	//Read the NSF data
	if(loadData){
		SAFE_NEW(nsf->dataBuffer,uint8_t,len,1);
		fread(nsf->dataBuffer,len,1,file);
		nsf->dataBufferSize = len;
	}

	//If the function succeeded to this point, then it was a successful read
	return 0;
}

int NsfFile_loadFile_NSFE(struct NsfFile* nsf,FILE* file,bool loadData){
	//Reset file read pos
	fseek(file,0,SEEK_SET);

	//Allocate and initialize vars
	NsfeChunkType chunkType[CHUNKTYPE_LENGTH];
	int chunkSize,
	    chunkUsed,
	    dataPos = 0;
	bool infoFound = false,
	     bankFound = false;

	struct NsfeInfoChunk info;
	memset(&info,'\0',sizeof(struct NsfeInfoChunk));
	info.trackCount = 1;//Default values

	//Read the header type
	fread(chunkType,4,1,file);

	//Begin reading chunks
	while(true){
		if(feof(file))
			return -1;

		fread(&chunkSize,4,1,file);
		fread(chunkType,4,1,file);

		if(memcmp(chunkType,CHUNKTYPE_INFO,CHUNKTYPE_LENGTH)==0){
			if(infoFound)//Restrict to one info chunk
				return -2;
			if(chunkSize<8)//Restrict to a minimum size of a chunk
				return -3;

			infoFound = true;
			chunkUsed = min((int)sizeof(struct NsfeInfoChunk),chunkSize);

			fread(&info,chunkUsed,1,file);
			fseek(file,chunkSize-chunkUsed,SEEK_CUR);

			nsf->isExtended  = 1;
			nsf->region = info.region & 3;
			nsf->loadAddress = info.loadAddress;
			nsf->initAddress = info.initAddress;
			nsf->playAddress = info.playAddress;
			nsf->chipExtensions = info.chipExtensions;
			nsf->trackCount = info.trackCount;
			nsf->initialTrack = info.initialTrack;

			nsf->palPlaySpeed  = (uint16_t)(1000000/PAL_NMIRATE); //blarg
			nsf->ntscPlaySpeed = (uint16_t)(1000000/NTSC_NMIRATE);//blarg
		}else if(memcmp(chunkType,CHUNKTYPE_DATA,CHUNKTYPE_LENGTH)==0){
			if(!infoFound)
				return -4;
			if(dataPos)
				return -5;
			if(chunkSize<1)
				return -6;

			nsf->dataBufferSize = chunkSize;
			dataPos = ftell(file);

			fseek(file,chunkSize,SEEK_CUR);
		}else if(memcmp(chunkType,CHUNKTYPE_NEND,CHUNKTYPE_LENGTH)==0){
			break;
		}else if(memcmp(chunkType,CHUNKTYPE_TIME,CHUNKTYPE_LENGTH)==0){
			if(!infoFound)
				return -7;
			if(nsf->trackTimes)
				return -8;

			SAFE_NEW(nsf->trackTimes,int,nsf->trackCount,1);
			chunkUsed = min(chunkSize/4,nsf->trackCount);

			fread(nsf->trackTimes,chunkUsed,4,file);
			fseek(file,chunkSize-(chunkUsed*4),SEEK_CUR);

			for(;chunkUsed<nsf->trackCount;++chunkUsed)
				nsf->trackTimes[chunkUsed] = -1;	//negative signals to use default time
		}else if(memcmp(chunkType,CHUNKTYPE_FADE,CHUNKTYPE_LENGTH)==0){
			if(!infoFound)
				return -9;
			if(nsf->trackFades)
				return -10;

			SAFE_NEW(nsf->trackFades,int,nsf->trackCount,1);
			chunkUsed = min(chunkSize / 4,nsf->trackCount);

			fread(nsf->trackFades,chunkUsed,4,file);
			fseek(file,chunkSize - (chunkUsed * 4),SEEK_CUR);

			for(;chunkUsed<nsf->trackCount;++chunkUsed)
				nsf->trackFades[chunkUsed] = -1;	//negative signals to use default time
		}else if(memcmp(chunkType,CHUNKTYPE_BANK,CHUNKTYPE_LENGTH)==0){
			if(bankFound)
				return -11;

			bankFound = 1;
			chunkUsed = min(8,chunkSize);

			fread(nsf->bankSwitch,chunkUsed,1,file);
			fseek(file,chunkSize - chunkUsed,SEEK_CUR);
		}else if(memcmp(chunkType,CHUNKTYPE_PLST,CHUNKTYPE_LENGTH)==0){
			if(nsf->playlist)
				return -12;

			nsf->playlistSize = chunkSize;
			if(nsf->playlistSize < 1)
				break;  //no playlist?

			SAFE_NEW(nsf->playlist,uint8_t,nsf->playlistSize,1);
			fread(nsf->playlist,chunkSize,1,file);
		}else if(memcmp(chunkType,CHUNKTYPE_AUTH,CHUNKTYPE_LENGTH)==0){
			if(nsf->gameTitle)
				return -13;

			char*		buffer;
			char*		ptr;
			SAFE_NEW(buffer,char,chunkSize + 4,1);

			fread(buffer,chunkSize,1,file);
			ptr = buffer;

			char** ar[4] = {&nsf->gameTitle,&nsf->artist,&nsf->copyright,&nsf->ripper};
			for(int i=0;i<4;++i){
				chunkUsed = strlen(ptr) + 1;
				*ar[i] = malloc(chunkUsed);
				if(!*ar[i]) { SAFE_DELETE(buffer); return 0; }
				memcpy(*ar[i],ptr,chunkUsed);
				ptr += chunkUsed;
			}
			SAFE_DELETE(buffer);
		}else if(memcmp(chunkType,CHUNKTYPE_TLBL,CHUNKTYPE_LENGTH)==0){
			if(!infoFound)
				return -14;
			if(nsf->trackLabels)
				return -15;

			SAFE_NEW(nsf->trackLabels,char*,nsf->trackCount,1);

			char* buffer;
			char* ptr;
			SAFE_NEW(buffer,char,chunkSize + nsf->trackCount,1);

			fread(buffer,chunkSize,1,file);
			ptr = buffer;

			for(int i=0;i<nsf->trackCount;++i){
				chunkUsed = strlen(ptr) + 1;
				nsf->trackLabels[i] = malloc(chunkUsed);
				if(!nsf->trackLabels[i]){
					SAFE_DELETE(buffer);
					return 0;
				}
				memcpy(nsf->trackLabels[i],ptr,chunkUsed);
				ptr += chunkUsed;
			}
			SAFE_DELETE(buffer);
		}else{//Unknown chunk
			if(chunkType[0]>='A' && chunkType[0]<='Z')//If it is a required chunk, don't continue
				return -16;
			//otherwise, just skip it
			fseek(file,chunkSize,SEEK_CUR);
		}
	}

	//if we exited the while loop without a 'return', we must have hit an NEND chunk
	//if this is the case, the file was layed out as it was expected.
	//now.. make sure we found both an info chunk, AND a data chunk... since these are
	//minimum requirements for a valid NSFE file
	if(!infoFound)
		return -17;
	if(!dataPos)
		return -18;

	//if both those chunks existed, this file is valid.  Load the data if it's needed
	if(loadData){
		fseek(file,dataPos,SEEK_SET);
		SAFE_NEW(nsf->dataBuffer,uint8_t,nsf->dataBufferSize,1);
		fread(nsf->dataBuffer,nsf->dataBufferSize,1,file);
	}
	else
		nsf->dataBufferSize = 0;

	//return success!
	return 0;
}

int NsfFile_load(struct NsfFile* nsf,FILE* file,bool loadData,bool ignoreversion){
	if(!file)
		return -1;

	char type[HEADERTYPE_LENGTH];
	fread(&type,4,1,file);

	if(memcmp(type,HEADERTYPE_NESM,HEADERTYPE_LENGTH)==0)
		return NsfFile_loadFile_NESM(nsf,file,loadData,ignoreversion);
	else if(memcmp(type,HEADERTYPE_NSFE,HEADERTYPE_LENGTH)==0)
		return NsfFile_loadFile_NSFE(nsf,file,loadData);
	else
		return -1;

	// Snake's revenge puts '00' for the initial track, which (after subtracting 1) makes it 256 or -1 (bad!)
	// This prevents that crap
	if(nsf->initialTrack >= nsf->trackCount)
		nsf->initialTrack = 0;
	if(nsf->initialTrack < 0)
		nsf->initialTrack = 0;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//  File saving
/*
int NsfFile_saveFile(struct NsfFile* nsf,const char* path){
	if(!nsf->dataBuffer)
//if we didn't grab the data, we can't save it
		return 1;

	FILE* file = fopen(path,"wb");
	if(file == NULL) return 1;

	int ret;
	if(nsf->isExtended)
ret = SaveFile_NSFE(file);
	else ret = SaveFile_NESM(file);

	fclose(file);
	return ret;
}

int NsfFile_saveFile_NESM(struct NsfFile* nsf,FILE* file){
	struct NesmHeader header;
	ZeroMemory(&header,0x80);

	header.type = HEADERTYPE_NESM;
	header.typeExtra = 0x1A;
	header.version = 1;
	header.trackCount = nsf->trackCount;
	header.initialTrack = nsf->initialTrack + 1;
	header.loadAddress = nsf->loadAddress;
	header.initAddress = nsf->initAddress;
	header.playAddress = nsf->playAddress;

	if(nsf->gameTitle)
memcpy(header.gameTitle,nsf->gameTitle,min(strlen(nsf->gameTitle),31));
	if(nsf->artist)
memcpy(header.artist   ,nsf->artist   ,min(strlen(nsf->artist)   ,31));
	if(nsf->copyright)
memcpy(header.copyright,nsf->copyright,min(strlen(nsf->copyright),31));

	header.speedNTSC = nsf->ntscPlaySpeed;
	memcpy(header.bankSwitch,nsf->bankSwitch,8);
	header.speedPAL = nsf->palPlaySpeed;
	header.region = nsf->region;
	header.chipExtensions = nsf->chipExtensions;

	//the header is all set... slap it in
	fwrite(&header,0x80,1,file);

	//slap in the NSF info
	fwrite(nsf->dataBuffer,nsf->dataBufferSize,1,file);

	//we're done.. all the other info that isn't recorded is dropped for regular NSFs
	return 0;
}

int NsfFile_saveFile_NSFE(struct NsfFile* nsf,FILE* file){
	//////////////////////////////////////////////////////////////////////////
	// I must admit... NESM files are a bit easier to work with than NSFEs =P

	uint chunkType;
	int chunkSize;
	struct NsfeInfoChunk info;

	//write the header
	chunkType = HEADERTYPE_NSFE;
	fwrite(&chunkType,4,1,file);


	//write the info chunk
	chunkType = CHUNKTYPE_INFO;
	chunkSize = sizeof(struct NsfeInfoChunk);
	info.chipExtensions = nsf->chipExtensions;
	info.initAddress = nsf->initAddress;
	info.region = nsf->region;
	info.loadAddress = nsf->loadAddress;
	info.playAddress = nsf->playAddress;
	info.initialTrack = nsf->initialTrack;
	info.trackCount = nsf->trackCount;

	fwrite(&chunkSize,4,1,file);
	fwrite(&chunkType,4,1,file);
	fwrite(&info,chunkSize,1,file);

	//if we need bankswitching... throw it in
	for(chunkSize=0;chunkSize<8;++chunkSize){
		if(nsf->bankSwitch[chunkSize]){
			chunkType = CHUNKTYPE_BANK;
			chunkSize = 8;
			fwrite(&chunkSize,4,1,file);
			fwrite(&chunkType,4,1,file);
			fwrite(nsf->bankSwitch,chunkSize,1,file);
			break;
		}
	}

	//if there's a time chunk, slap it in
	if(nsf->trackTimes){
		chunkType = CHUNKTYPE_TIME;
		chunkSize = 4 * nsf->trackCount;
		fwrite(&chunkSize,4,1,file);
		fwrite(&chunkType,4,1,file);
		fwrite(nsf->trackTimes,chunkSize,1,file);
	}

	//slap in a fade chunk if needed
	if(nsf->trackFades){
		chunkType = CHUNKTYPE_FADE;
		chunkSize = 4 * nsf->trackCount;
		fwrite(&chunkSize,4,1,file);
		fwrite(&chunkType,4,1,file);
		fwrite(nsf->trackFades,chunkSize,1,file);
	}

	//auth!
	if(nsf->gameTitle || nsf->copyright || nsf->artist || nsf->ripper){
		chunkType = CHUNKTYPE_AUTH;
		chunkSize = 4;
		if(nsf->gameTitle)
chunkSize += strlen(nsf->gameTitle);
		if(nsf->artist)
chunkSize += strlen(nsf->artist);
		if(nsf->copyright)
chunkSize += strlen(nsf->copyright);
		if(nsf->ripper)
chunkSize += strlen(nsf->ripper);
		fwrite(&chunkSize,4,1,file);
		fwrite(&chunkType,4,1,file);

		if(nsf->gameTitle)
fwrite(nsf->gameTitle,strlen(nsf->gameTitle) + 1,1,file);
		else fwrite("",1,1,file);
		if(nsf->artist)
fwrite(nsf->artist,strlen(nsf->artist) + 1,1,file);
		else fwrite("",1,1,file);
		if(nsf->copyright)
fwrite(nsf->copyright,strlen(nsf->copyright) + 1,1,file);
		else fwrite("",1,1,file);
		if(nsf->ripper)
fwrite(nsf->ripper,strlen(nsf->ripper) + 1,1,file);
		else fwrite("",1,1,file);
	}

	//plst
	if(nsf->playlist){
		chunkType = CHUNKTYPE_PLST;
		chunkSize = nsf->playlistSize;
		fwrite(&chunkSize,4,1,file);
		fwrite(&chunkType,4,1,file);
		fwrite(nsf->playlist,chunkSize,1,file);
	}

	//tlbl
	if(nsf->trackLabels){
		chunkType = CHUNKTYPE_TLBL;
		chunkSize = nsf->trackCount;

		for(int i=0;i<nsf->trackCount;++i)
			chunkSize += strlen(nsf->trackLabels[i]);

		fwrite(&chunkSize,4,1,file);
		fwrite(&chunkType,4,1,file);

		for(int i=0;i<nsf->trackCount;++i){
			if(nsf->trackLabels[i])
				fwrite(nsf->trackLabels[i],strlen(nsf->trackLabels[i]) + 1,1,file);
			else
				fwrite("",1,1,file);
		}
	}

	//data
	chunkType = CHUNKTYPE_DATA;
	chunkSize = nsf->dataBufferSize;
	fwrite(&chunkSize,4,1,file);
	fwrite(&chunkType,4,1,file);
	fwrite(nsf->dataBuffer,chunkSize,1,file);

	//END
	chunkType = CHUNKTYPE_NEND;
	chunkSize = 0;
	fwrite(&chunkSize,4,1,file);
	fwrite(&chunkType,4,1,file);

	//w00t
	return 0;
}*/