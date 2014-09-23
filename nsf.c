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
#define SAFE_NEW(ptr,type,n,failureReturn) if(!(ptr=malloc(sizeof(type)*n)))return failureReturn;memset(ptr,'\0',sizeof(type)*n)

void nsf_free(const struct nsf_data* nsf){
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

int nsf_loadNesm(struct nsf_data* nsf,FILE* file,bool loadData){
	//Read the header (Copy the whole header to the structure)
	struct nsf_nesmHeader header;
	fread(&header,sizeof(struct nsf_nesmHeader),1,file);

	//Confirm the header type for NESM
	if(memcmp(header.type,NSF_HEADERTYPE_NESM,NSF_HEADERTYPE_LENGTH)!=0)
		return -2;
	if(header.typeExtra!=0x1A)
		return -3;
	if(header.version!=1)//TODO: Some badly formatted NSFs claims to be above version 1
		return -4;

	//Copy data from nsf_nesmHeader to nsf_data
	nsf->region         = header.region & 0x03;
	nsf->palPlaySpeed   = header.speedPAL; //blarg
	nsf->ntscPlaySpeed  = header.speedNTSC;//blarg
	nsf->loadAddress    = header.loadAddress;
	nsf->initAddress    = header.initAddress;
	nsf->playAddress    = header.playAddress;
	nsf->chipExtensions = header.chipExtensions;

	nsf->trackCount   = header.trackCount;    //0-based number copied
	nsf->initialTrack = header.initialTrack-1;//1-based number from header converted to 0-based

	memcpy(nsf->bankSwitch,header.bankSwitch,sizeof(header.bankSwitch));

	//Allocate space for strings and +1 for null termination
	if(!(nsf->gameTitle = malloc(sizeof(header.gameTitle) + sizeof(char)*1)))
		return 1;
	if(!(nsf->artist    = malloc(sizeof(header.artist)    + sizeof(char)*1)))
		return 1;
	if(!(nsf->copyright = malloc(sizeof(header.copyright) + sizeof(char)*1)))
		return 1;

	//Copy strings and append a null terminator
	memcpy(nsf->gameTitle,header.gameTitle,sizeof(header.gameTitle));
	nsf->gameTitle[sizeof(header.gameTitle)] = '\0';
	
	memcpy(nsf->artist   ,header.artist   ,sizeof(header.artist));
	nsf->artist   [sizeof(header.artist)] = '\0';
	
	memcpy(nsf->copyright,header.copyright,sizeof(header.copyright));
	nsf->copyright[sizeof(header.copyright)] = '\0';

	//Read the NSF data
	if(loadData){
		fseek(file,0,SEEK_END);
		int len = ftell(file)-sizeof(struct nsf_nesmHeader);
		fseek(file,sizeof(struct nsf_nesmHeader),SEEK_SET);

		SAFE_NEW(nsf->dataBuffer,uint8_t,len,1);
		fread(nsf->dataBuffer,len,1,file);
		nsf->dataBufferSize = len;
	}

	//If the function have reached this point, then it was a successful read
	return 0;
}

int nsf_loadNsfe(struct nsf_data* nsf,FILE* file,bool loadData){
	//Allocate and initialize vars
	nsfe_chunkType chunkType[NSFE_CHUNKTYPE_LENGTH];
	int chunkSize,
	    chunkUsed,
	    dataPos = 0;
	bool infoFound = false,
	     bankFound = false;

	struct nsfe_infoChunk info;
	memset(&info,'\0',sizeof(struct nsfe_infoChunk));
	info.trackCount = 1;//Default values

	//Read the header type
	fread(chunkType,4,1,file);

	//Begin reading chunks
	while(true){
		if(feof(file))
			return -1;

		fread(&chunkSize,4,1,file);
		fread(chunkType,4,1,file);

		if(memcmp(chunkType,NSFE_CHUNKTYPE_INFO,NSFE_CHUNKTYPE_LENGTH)==0){
			if(infoFound)//Restrict to one info chunk
				return -2;
			if(chunkSize<8)//Restrict to a minimum size of a chunk
				return -3;

			infoFound = true;
			chunkUsed = min((int)sizeof(struct nsfe_infoChunk),chunkSize);

			fread(&info,chunkUsed,1,file);
			fseek(file,chunkSize-chunkUsed,SEEK_CUR);

			nsf->region = info.region & 3;
			nsf->loadAddress = info.loadAddress;
			nsf->initAddress = info.initAddress;
			nsf->playAddress = info.playAddress;
			nsf->chipExtensions = info.chipExtensions;
			nsf->trackCount = info.trackCount;
			nsf->initialTrack = info.initialTrack;

			nsf->palPlaySpeed  = (uint16_t)(1000000/PAL_NMIRATE); //blarg
			nsf->ntscPlaySpeed = (uint16_t)(1000000/NTSC_NMIRATE);//blarg
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_DATA,NSFE_CHUNKTYPE_LENGTH)==0){
			if(!infoFound)
				return -4;
			if(dataPos)
				return -5;
			if(chunkSize<1)
				return -6;

			nsf->dataBufferSize = chunkSize;
			dataPos = ftell(file);

			fseek(file,chunkSize,SEEK_CUR);
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_NEND,NSFE_CHUNKTYPE_LENGTH)==0){
			break;
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_TIME,NSFE_CHUNKTYPE_LENGTH)==0){
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
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_FADE,NSFE_CHUNKTYPE_LENGTH)==0){
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
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_BANK,NSFE_CHUNKTYPE_LENGTH)==0){
			if(bankFound)
				return -11;

			bankFound = 1;
			chunkUsed = min(8,chunkSize);

			fread(nsf->bankSwitch,chunkUsed,1,file);
			fseek(file,chunkSize - chunkUsed,SEEK_CUR);
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_PLST,NSFE_CHUNKTYPE_LENGTH)==0){
			if(nsf->playlist)
				return -12;

			nsf->playlistSize = chunkSize;
			if(nsf->playlistSize < 1)
				break;  //no playlist?

			SAFE_NEW(nsf->playlist,uint8_t,nsf->playlistSize,1);
			fread(nsf->playlist,chunkSize,1,file);
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_AUTH,NSFE_CHUNKTYPE_LENGTH)==0){
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
		}else if(memcmp(chunkType,NSFE_CHUNKTYPE_TLBL,NSFE_CHUNKTYPE_LENGTH)==0){
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

int nsf_load(struct nsf_data* nsf,FILE* file,bool loadData){
	if(!file)
		return -1;

	char type[NSF_HEADERTYPE_LENGTH];
	fread(&type,4,1,file);

	//Reset file read pos
	rewind(file);

	if(memcmp(type,NSF_HEADERTYPE_NESM,NSF_HEADERTYPE_LENGTH)==0)
		return nsf_loadNesm(nsf,file,loadData);
	else if(memcmp(type,NSF_HEADERTYPE_NSFE,NSF_HEADERTYPE_LENGTH)==0)
		return nsf_loadNsfe(nsf,file,loadData);
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

int nsf_saveNesm(const struct nsf_data* nsf,FILE* file){
	if(file==NULL)
		return -1;
	
	//Initialize header data and simply copying from the nsf
	struct nsf_nesmHeader header = {
		.type           = NSF_HEADERTYPE_NESM,
		.typeExtra      = 0x1A,
		.version        = 1,
		.trackCount     = nsf->trackCount,
		.initialTrack   = nsf->initialTrack + 1,
		.loadAddress    = nsf->loadAddress,
		.initAddress    = nsf->initAddress,
		.playAddress    = nsf->playAddress,
		.speedNTSC      = nsf->ntscPlaySpeed,
		.speedPAL       = nsf->palPlaySpeed,
		.region         = nsf->region,
		.chipExtensions = nsf->chipExtensions
	};

	//Copy strings and arrays of data
	if(nsf->gameTitle){
		memcpy(header.gameTitle,nsf->gameTitle,min(strlen(nsf->gameTitle),31));
		header.gameTitle[31]='\0';
	}else
		memset(header.gameTitle,'\0',32);

	if(nsf->artist){
		memcpy(header.artist,nsf->artist,min(strlen(nsf->artist),31));
		header.artist[31]='\0';
	}else
		memset(header.artist,'\0',32);

	if(nsf->copyright){
		memcpy(header.copyright,nsf->copyright,min(strlen(nsf->copyright),31));
		header.copyright[31]='\0';
	}
	else
		memset(header.copyright,'\0',32);

	if(nsf->bankSwitch)
		memcpy(header.bankSwitch,nsf->bankSwitch,8);
	else
		memset(header.bankSwitch,'\0',8);

	//Copy the header to the file
	fwrite(&header,sizeof(struct nsf_nesmHeader),1,file);

	//Copy the data
	if(nsf->dataBufferSize>0 && nsf->dataBuffer)
		fwrite(nsf->dataBuffer,nsf->dataBufferSize,1,file);

	return 0;
}

void nsfe_saveChunk(nsfe_chunkType type[NSFE_CHUNKTYPE_LENGTH],void* data,size_t size){

}

int nsf_saveNsfe(const struct nsf_data* nsf,FILE* file){
	if(file==NULL)
		return -1;

	int chunkSize;
	struct nsfe_infoChunk info;

	//Header type
	fwrite(NSF_HEADERTYPE_NSFE,4,1,file);


	//Info chunk
	chunkSize = sizeof(struct nsfe_infoChunk);
	info.chipExtensions = nsf->chipExtensions;
	info.initAddress    = nsf->initAddress;
	info.region         = nsf->region;
	info.loadAddress    = nsf->loadAddress;
	info.playAddress    = nsf->playAddress;
	info.initialTrack   = nsf->initialTrack;
	info.trackCount     = nsf->trackCount;

	fwrite(&chunkSize,4,1,file);
	fwrite(NSFE_CHUNKTYPE_INFO,4,1,file);
	fwrite(&info,chunkSize,1,file);

	//Bankswitching chunk if needed
	for(chunkSize=0;chunkSize<8;++chunkSize){
		if(nsf->bankSwitch[chunkSize]){
			chunkSize = 8;
			fwrite(&chunkSize,4,1,file);
			fwrite(NSFE_CHUNKTYPE_BANK,4,1,file);
			fwrite(nsf->bankSwitch,chunkSize,1,file);
			break;
		}
	}

	//Time chunk if needed
	if(nsf->trackTimes){
		chunkSize = 4 * nsf->trackCount;
		fwrite(&chunkSize,4,1,file);
		fwrite(NSFE_CHUNKTYPE_TIME,4,1,file);
		fwrite(nsf->trackTimes,chunkSize,1,file);
	}

	//Fade chunk if needed
	if(nsf->trackFades){
		chunkSize = 4 * nsf->trackCount;
		fwrite(&chunkSize,4,1,file);
		fwrite(NSFE_CHUNKTYPE_FADE,4,1,file);
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
		fwrite(&chunkSize,4,1,file);
		fwrite(NSFE_CHUNKTYPE_AUTH,4,1,file);

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
		chunkSize = nsf->playlistSize;
		fwrite(&chunkSize,4,1,file);
		fwrite(NSFE_CHUNKTYPE_PLST,4,1,file);
		fwrite(nsf->playlist,chunkSize,1,file);
	}

	//tlbl chunk
	if(nsf->trackLabels){
		chunkSize = nsf->trackCount;

		for(int i=0;i<nsf->trackCount;++i)
			chunkSize += strlen(nsf->trackLabels[i]);

		fwrite(&chunkSize,4,1,file);
		fwrite(NSFE_CHUNKTYPE_TLBL,4,1,file);

		for(int i=0;i<nsf->trackCount;++i){
			if(nsf->trackLabels[i])
				fwrite(nsf->trackLabels[i],strlen(nsf->trackLabels[i]) + 1,1,file);
			else
				putc('\0',file);
		}
	}

	//Data
	chunkSize = nsf->dataBufferSize;
	fwrite(&chunkSize,4,1,file);
	fwrite(NSFE_CHUNKTYPE_DATA,4,1,file);
	fwrite(nsf->dataBuffer,chunkSize,1,file);

	//end chunk
	chunkSize = 0;
	fwrite(&chunkSize,4,1,file);
	fwrite(NSFE_CHUNKTYPE_NEND,4,1,file);

	//w00t
	return 0;
}
