#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "nsf.h"
#include <popt.h>//TODO: Rewrite to argp because it appears to be GNU standard and more widespread

void nsf_printChipExtensions(const nsf_chipExtensions* ext,FILE* output){
	if(*ext&NSF_CHIP_VRCVI)       fputs("VRCVI, ",output);
	if(*ext&NSF_CHIP_VRCVII)      fputs("VRCVII, ",output);
	if(*ext&NSF_CHIP_FDS)         fputs("FDS sound, ",output);
	if(*ext&NSF_CHIP_MMC5)        fputs("MMC5 audio, ",output);
	if(*ext&NSF_CHIP_NAMCO106)    fputs("Namco 106, ",output);
	if(*ext&NSF_CHIP_SUNSOFT_FME07)fputs("Sunsoft FME-07, ",output);
}

struct nsf_data nsf;
FILE* in  = NULL,
    * out = NULL;

const char* filepath_in  = NULL,
          * filepath_out = NULL;

bool output=false;

const char const EMPTY_STRING[] = "\0";

struct{
	const char* title,
              * region,
              * artist,
              * copyright,
              * ripper;
}write={NULL,NULL,NULL,NULL,NULL};

struct{
	uint8_t title        :1;
	uint8_t region       :1;
	uint8_t artist       :1;
	uint8_t copyright    :1;
	uint8_t ripper       :1;
	uint8_t trackcount   :1;
	uint8_t initialtrack :1;
	uint8_t extensions   :1;
}show={false,false,false,false,false,false,false,false};

poptContext parameterContext;

enum Show{
	SHOW_NULL,
	SHOW_TITLE,
	SHOW_REGION,
	SHOW_ARTIST,
	SHOW_COPYRIGHT,
	SHOW_RIPPER,
	SHOW_TRACKCOUNT,
	SHOW_INITIALTRACK,
	SHOW_EXTENSIONS,
	SHOW_OUTPUT
};

int main(int argc,const char* argv[]){
	//Initialize variables
	memset(&nsf,'\0',sizeof(struct nsf_data));

	//popt
	struct poptOption parameterTable[] = {
		{
			"title",
			't',
			POPT_ARG_STRING | POPT_ARGFLAG_OPTIONAL,
			&write.title,
			SHOW_TITLE,
			"Title of nsf.",
			"<string>"
		},
		{
			"region",
			'r',
			POPT_ARG_STRING | POPT_ARGFLAG_OPTIONAL,
			&write.region,
			SHOW_REGION,
			"Region of nsf.",
			"NTSC|PAL|MIX1|MIX2"
		},
		{
			"artist",
			'a',
			POPT_ARG_STRING | POPT_ARGFLAG_OPTIONAL,
			&write.artist,
			SHOW_ARTIST,
			"Artist of nsf.",
			"<string>"
		},
		{
			"copyright",
			'c',
			POPT_ARG_STRING | POPT_ARGFLAG_OPTIONAL,
			&write.copyright,
			SHOW_COPYRIGHT,
			"Copyright string of nsf.",
			"<string>"
		},
		{
			"ripper",
			'p',
			POPT_ARG_STRING | POPT_ARGFLAG_OPTIONAL,
			&write.ripper,
			SHOW_RIPPER,
			"Region of nsf.",
			"<string>"
		},
		{
			"out",
			'o',
			POPT_ARG_STRING,
			&filepath_out,
			SHOW_OUTPUT,
			"Output filepath.",
			"<filepath>"
		},
		{
			"trackcount",
			'l',
			POPT_ARG_NONE,
			NULL,
			SHOW_TRACKCOUNT,
			"Amount of tracks in the nsf.",
			NULL
		},
		{
			"initialtrack",
			'i',
			POPT_ARG_NONE,
			NULL,
			SHOW_INITIALTRACK,
			"The initial track number (Output is a zero based integer).",
			NULL
		},
		{
			"extensions",
			'e',
			POPT_ARG_NONE,
			NULL,
			SHOW_EXTENSIONS,
			"The extension chips needed to play the nsf (Output is comma separated).",
			NULL
		},
		POPT_AUTOHELP
		POPT_TABLEEND
	};

	parameterContext = poptGetContext(NULL,argc,argv,parameterTable,0);
	poptSetOtherOptionHelp(parameterContext,"<path to nsf file> [OPTIONS...]");

	//Show help if there's no parameters
	if(argc<2){
		poptPrintUsage(parameterContext,stderr,0);
		return 1;
	}

	//Parse option parameters
	OptionLoop:{
		char c=poptGetNextOpt(parameterContext);
		switch(c){
			case POPT_ERROR_BADOPT:
				fputs("Error: Invalid parameter option\n",stderr);
				return POPT_ERROR_BADOPT;
			case POPT_ERROR_BADNUMBER :
				fputs("Error: Bad number\n",stderr);
				return POPT_ERROR_BADNUMBER ;
			case POPT_ERROR_OVERFLOW:
				fputs("Error: Number overflow\n",stderr);
				return POPT_ERROR_OVERFLOW;

			case -1:
				break;

			case SHOW_TITLE:
				show.title=true;
				goto OptionLoop;
			case SHOW_REGION:
				show.region=true;
				goto OptionLoop;
			case SHOW_ARTIST:
				show.artist=true;
				goto OptionLoop;
			case SHOW_COPYRIGHT:
				show.copyright=true;
				goto OptionLoop;
			case SHOW_RIPPER:
				show.ripper=true;
				goto OptionLoop;
			case SHOW_TRACKCOUNT:
				show.trackcount=true;
				goto OptionLoop;
			case SHOW_INITIALTRACK:
				show.initialtrack=true;
				goto OptionLoop;
			case SHOW_EXTENSIONS:
				show.extensions=true;
				goto OptionLoop;
			case SHOW_OUTPUT:
				output=true;
				goto OptionLoop;
			default:
				fputs("Error: Internal error\n",stderr);
				return 3;
		}
	}

	//Parse file parameter and do the thing
	if((filepath_in = poptGetArg(parameterContext))==NULL){
		poptPrintUsage(parameterContext,stderr,0);
		fputs("Specify the path to the nsf file: .e.g., /path/to/the/file.nsf\n",stderr);
		return 2;
	}else{
		in = fopen(filepath_in,"rb");
		int fileLoadingReturnCode=nsf_load(&nsf,in,output);

		if(fileLoadingReturnCode!=0){
			fprintf(stderr,"Error in file loading: %i\n",fileLoadingReturnCode);
			return 3;
		}

		if(argc==2){//If only the filepath is defined (nsfinfo /path/to/file.nsf)
			printf("Title: %s\nRegion: %s\nArtist: %s\nCopyright: %s\n\nTracks: %i\nInitial track: %i\nExtension Chips: ",
				nsf.gameTitle,
				nsf.info.region?"PAL":"NTSC",
				nsf.artist,
				nsf.copyright,
				nsf.info.trackCount,
				nsf.info.initialTrack
			);

			nsf_printChipExtensions(&nsf.info.chipExtensions,stdout);
			printf("\nRipper: %s\n\n",nsf.ripper);

			puts("Tracks:");
			for(int i=0;i<nsf.info.trackCount;++i){
				printf(" %03u:\n  Title:  %s\n  Length: %i ms (%u:%02u min)\n  Fade:   %i ms\n",
					i,
					nsf.trackLabels?nsf.trackLabels[i]:"",
					nsf.trackTimes?nsf.trackTimes[i]:-1,
					nsf.trackTimes?(nsf.trackTimes[i]/1000/60):0,
					nsf.trackTimes?(nsf.trackTimes[i]/1000%60):0,
					nsf.trackFades?nsf.trackFades[i]:-1
				);
			}

			if(nsf.playlist){
				puts("\nPlaylist Order:");
				for(int i=0;i<nsf.playlistSize;++i){
					printf(" %03u\n",nsf.playlist[i]);
				}
			}
		}else{
			//Write
			if(write.title)
				strcpy(nsf.gameTitle,write.title);

			if(write.region){
				if(strcmp(write.region,"NTSC")==0)
					nsf.info.region=NSF_REGION_NTSC;
				else if(strcmp(write.region,"PAL")==0)
					nsf.info.region=NSF_REGION_PAL;
				else{
					printf("Error: Invalid region: `%s`\n",write.region);
					return 5;
				}
			}

			if(write.artist)
				strcpy(nsf.artist,write.artist);

			if(write.copyright)
				strcpy(nsf.copyright,write.copyright);

			if(write.ripper)
				strcpy(nsf.ripper,write.ripper);

			//Show
			if(show.title)
				puts(nsf.gameTitle?:EMPTY_STRING);
			
			if(show.region)
				puts(nsf.info.region==1?"PAL":"NTSC");
			
			if(show.artist)
				puts(nsf.artist?:EMPTY_STRING);
			
			if(show.copyright)
				puts(nsf.copyright?:EMPTY_STRING);
			
			if(show.trackcount)
				printf("%i\n",nsf.info.trackCount);
			
			if(show.initialtrack)
				printf("%i\n",nsf.info.initialTrack);
			
			if(show.ripper)
				puts(nsf.ripper?:EMPTY_STRING);
			
			if(show.extensions){
				nsf_printChipExtensions(&nsf.info.chipExtensions,stdout);
				putchar('\n');
			}

			//Output file
			if(filepath_out){
				FILE* file=fopen(filepath_out,"wb");
				//nsf_saveNesm(&nsf,file);
				nsf_saveNsfe(&nsf,file);
				fclose(file);
			}
		}

		if(in){
			nsf_free(&nsf);
			fclose(in);
		}
		return fileLoadingReturnCode;
	}

	//Free popt context
	poptFreeContext(parameterContext);

	return EXIT_SUCCESS;
}
