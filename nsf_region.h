#ifndef __LOLIROFLE_LIBNSF_nsf_region_H_INCLUDED__
#define __LOLIROFLE_LIBNSF_nsf_region_H_INCLUDED__

#define NTSC_FREQUENCY 1789772.727273f
#define PAL_FREQUENCY  1652097.692308f

//Playback timing info
//To avoid sound playback timing issues, use these frequencies to call the play subroutine in the NSF.
#define NTSC_NMIRATE 60.098814f //21477272.72 / (261*1364 + (1360+1364)/2) (262 scanlines per frame, 1364 cycles per scanline (one scanlinegoes between 1360 and 1364 each frame))
#define PAL_NMIRATE  50.006982f //26601714 / (312*1705)

//0 = NTSC, 1 = PAL, 2,3 = mixed NTSC/PAL (interpreted as NTSC)
typedef uint8_t nsf_region;
#define NSF_REGION_NTSC 0
#define NSF_REGION_PAL  1
#define NSF_REGION_MIX1 2
#define NSF_REGION_MIX2 3

#endif
