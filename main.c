#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "nsf.h"
#include <popt.h>//TODO: Rewrite to argp because it appears to be GNU standard and more widespread

void nsf_printChipExtensions(const nsf_chipExtensions* ext,FILE* output){
	if(*ext & NSF_CHIP_VRCVI)        fputs("VRCVI, ",output);
	if(*ext & NSF_CHIP_VRCVII)       fputs("VRCVII, ",output);
	if(*ext & NSF_CHIP_FDS)          fputs("FDS sound, ",output);
	if(*ext & NSF_CHIP_MMC5)         fputs("MMC5 audio, ",output);
	if(*ext & NSF_CHIP_NAMCO106)     fputs("Namco 106, ",output);
	if(*ext & NSF_CHIP_SUNSOFT_FME07)fputs("Sunsoft FME-07, ",output);
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
              * ripper,
              * trackTitles;
}write={NULL,NULL,NULL,NULL,NULL};

struct{
	uint8_t title        :1;
	uint8_t region       :1;
	uint8_t artist       :1;
	uint8_t copyright    :1;
	uint8_t ripper       :1;
	uint8_t trackTitles  :1;
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
	SHOW_TRACKTITLES,
	SHOW_TRACKCOUNT,
	SHOW_INITIALTRACK,
	SHOW_EXTENSIONS,
	SHOW_OUTPUT
};

/**
 * Parses a string in a certain format, executing a function for each key-value pair.
 * Example of the format: 0:Zero,2,3:Three,4,5,7,8:Eight,100:"One hundred"
 *
 * @param str         Input, to parse from.
 *                    Must be non-null and a null-terminated string.
 * @param parsePair   Parsing function to execute.
 *                    The second argument is a pointer which may point to invalid data after the execution of parsePair.
 *                    Must be non-null.
 * @param closureData Passed to every execution of parsePair.
 */
bool parseOptionalIntStrMap(char* str,bool(*parsePair)(int,const char*,void*),void* closureData){
	char* tmp      = str;
	char* intBegin = NULL;
	char* intEnd   = NULL;
	char* strBegin = NULL;

	#define SKIP_SPACES() while(*tmp==' '){++tmp;}

	ParseKeyValue:
		SKIP_SPACES();
		if(*tmp=='\0') return true;

		//Parse int
		if(*tmp>='0' && *tmp<='9'){
			intBegin = tmp;

			while(*tmp>='0' && *tmp<='9'){
				++tmp;
			}

			intEnd = tmp;
		}else{
			return false;
		}

		SKIP_SPACES();

		//Parse separator
		switch(*tmp){
			//Key-value separator
			case ',':
				{
					char c = *intEnd;
					*intEnd = '\0';
					parsePair(atoi(intBegin),NULL,closureData);
					*intEnd = c;
				}

				++tmp;

				//Reset states, and begin parsing a new key-value
				intBegin = NULL;
				intEnd   = NULL;
				goto ParseKeyValue;

			//End of string
			case '\0':
				parsePair(atoi(intBegin),NULL,closureData);
				return true;

			//Continue to parsing the value
			case ':':
				++tmp;
				break;

			//Expected separator or end-of-string
			default:
				return false;
		}

		SKIP_SPACES();

		//Parse str
		strBegin = tmp;
		switch(*tmp){
			//TODO: String with citation marks
			/*case '"':
				ParseValue:
					switch(*++tmp){
						case '\0':
							return false;
						case '"':
							++tmp;
							SKIP_SPACES();
							if(*tmp=='\0') return true;
							break;
						default:
							goto ParseValue;
					}
				break;*/

			//String without citation marks
			default: ParseValueRaw:
				switch(*++tmp){
					case ',':
						{
							char c = *intEnd;
							*intEnd = '\0';
							*tmp    = '\0';
							parsePair(atoi(intBegin),strBegin,closureData);
							*tmp    = ',';
							*intEnd = c;
						}

						++tmp;

						//Reset states, and begin parsing a new key-value
						intBegin = NULL;
						intEnd   = NULL;
						strBegin = NULL;
						goto ParseKeyValue;

					case '\0':
						{
							char c = *intEnd;
							*intEnd = '\0';
							parsePair(atoi(intBegin),strBegin,closureData);
							*intEnd = c;
						}

						return true;

					default:
						goto ParseValueRaw;
				}
				break;
		}
}

char* strcpyof(const char* str){
	char* new = malloc(strlen(str));
	if(new==NULL) return NULL;
	strcpy(new,str);
	return new;
}

struct TrackTitleClosureData{
	char** titles;
	uint8_t tracks;
};
bool trackTitleClosure(int i,const char* title,void* dat){
	struct TrackTitleClosureData* data = dat;

	if(0<=i && i<data->tracks){
		//Write
		if(title!=NULL){
			if((data->titles[i] = strcpyof(title))==NULL) return false;
		}

		//Show
		puts(data->titles[i]);

		return true;
	}else{
		return false;
	}
}

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
			"Title of nsf. Often used as the album name.",
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
			"String of the ripper of the nsf.",
			"<string>"
		},
		{
			"tracktitles",
			'T',
			POPT_ARG_STRING | POPT_ARGFLAG_OPTIONAL,
			&write.trackTitles,
			SHOW_TRACKTITLES,
			"Titles of tracks.",
			"<int>[:<string>][,..]"
		},
		{
			"out",
			'o',
			POPT_ARG_STRING,
			&filepath_out,
			SHOW_OUTPUT,
			"Output filepath. This must be specified for changes to be written out.",
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
			case SHOW_TRACKTITLES:
				show.trackTitles=true;
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
			if(write.title){
				if((nsf.gameTitle = strcpyof(write.title))==NULL) return 1;
			}

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

			if(write.artist){
				if((nsf.artist = strcpyof(write.artist))==NULL) return 1;
			}

			if(write.copyright){
				if((nsf.copyright = strcpyof(write.copyright))==NULL) return 1;
			}

			if(write.ripper){
				if((nsf.ripper = strcpyof(write.ripper))==NULL) return 1;
			}

			if(write.trackTitles){
				struct TrackTitleClosureData data = {nsf.trackLabels,nsf.info.trackCount};//TODO: Is it possible that nsf.trackLabels=NULL?
				parseOptionalIntStrMap(write.trackTitles,trackTitleClosure,&data);
			}

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
