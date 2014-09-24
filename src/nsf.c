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

void nsf_init(struct nsf_data* nsf){
	//Set to zeroes in case of error (when it is neccessary to free)
	memset(nsf,0,sizeof(struct nsf_data));
}

void nsf_free(const struct nsf_data* nsf){
	if(nsf==NULL)
		return;

	free(nsf->dataBuffer);
	free(nsf->playlist);
	free(nsf->trackTimes);
	free(nsf->trackFades);
	if(nsf->trackLabels){
		for(int i=0;i<nsf->info.trackCount;++i)
			free(nsf->trackLabels[i]);
		free(nsf->trackLabels);
	}
	free(nsf->gameTitle);
	free(nsf->artist);
	free(nsf->copyright);
	free(nsf->ripper);
}
