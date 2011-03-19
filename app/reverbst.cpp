/*
 *  reverbst.c -- mono reverberation filtration
 *
 *  Written and copywritten by Philip Edelbrock,
 *  Copyright (C) 1999
 *
 *  This file is part of AlsaPlayer.
 *
 *  AlsaPlayer is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AlsaPlayer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Version 1.0.1: updated licence to GPL3 or later
 *
 */

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "CorePlayer.h"

int fbper;
float decayamt;
long sampling=44100;

/* Sound card interface defs and vars */
#define SAMPLING_RATE sampling

int audio_fd;  

/* Left channel Filter design parameters */
#define COMB_L_GAIN_1 (double)-0.40
#define COMB_L_DELAY_1 (-(log10(-(COMB_L_GAIN_1)) * decayamt * SAMPLING_RATE)/ 3.0)

#define COMB_L_GAIN_2 (double)-0.42
#define COMB_L_DELAY_2 (-(log10(-(COMB_L_GAIN_2)) * decayamt * SAMPLING_RATE)/ 3.0)

#define COMB_L_GAIN_3 (double)-0.44
#define COMB_L_DELAY_3 (-(log10(-(COMB_L_GAIN_3)) * decayamt * SAMPLING_RATE)/ 3.0)

#define COMB_L_GAIN_4 (double)-0.60
#define COMB_L_DELAY_4 (-(log10(-(COMB_L_GAIN_4)) * decayamt * SAMPLING_RATE)/ 3.0)

const double COMB_L_GAIN[4]={COMB_L_GAIN_1,COMB_L_GAIN_2,COMB_L_GAIN_3,COMB_L_GAIN_4};
long COMB_L_DELAY[4];

#define MAXDELAY (long)24000


/* Global vars for filters */

double lmem1[MAXDELAY];
double lmem2[MAXDELAY];
double lmem3[MAXDELAY];
double lmem4[MAXDELAY];

long lstep[4]={0,0,0,0};

double lallpassmem1=0;
double lallpassmem2=0;


/* Right channel Filter design parameters */
#define COMB_R_GAIN_1 (double)-0.42
#define COMB_R_DELAY_1 (-(log10(-(COMB_R_GAIN_1)) * decayamt * SAMPLING_RATE)/ 3.0)

#define COMB_R_GAIN_2 (double)-0.47
#define COMB_R_DELAY_2 (-(log10(-(COMB_R_GAIN_2)) * decayamt * SAMPLING_RATE)/ 3.0)

#define COMB_R_GAIN_3 (double)-0.48
#define COMB_R_DELAY_3 (-(log10(-(COMB_R_GAIN_3)) * decayamt * SAMPLING_RATE)/ 3.0)

#define COMB_R_GAIN_4 (double)-0.59
#define COMB_R_DELAY_4 (-(log10(-(COMB_R_GAIN_4)) * decayamt * SAMPLING_RATE)/ 3.0)

const double COMB_R_GAIN[4]={COMB_R_GAIN_1,COMB_R_GAIN_2,COMB_R_GAIN_3,COMB_R_GAIN_4};
long COMB_R_DELAY[4];

/* Mixing value -- Value from 0 to 1 for the ammount of reverb */
#define AMMOUNT ((float)fbper/100.0)

/* Global vars for filters */

double rmem1[MAXDELAY];
double rmem2[MAXDELAY];
double rmem3[MAXDELAY];
double rmem4[MAXDELAY];


long rstep[4]={0,0,0,0};

double rallpassmem1=0;
double rallpassmem2=0;

/* Fill the comb histories with silence */
void initdelays(void) {
 COMB_R_DELAY[0]=(long)COMB_R_DELAY_1;
 COMB_R_DELAY[1]=(long)COMB_R_DELAY_2;
 COMB_R_DELAY[2]=(long)COMB_R_DELAY_3;
 COMB_R_DELAY[3]=(long)COMB_R_DELAY_4;
 COMB_L_DELAY[0]=(long)COMB_L_DELAY_1;
 COMB_L_DELAY[1]=(long)COMB_L_DELAY_2;
 COMB_L_DELAY[2]=(long)COMB_L_DELAY_3;
 COMB_L_DELAY[3]=(long)COMB_L_DELAY_4;
 #ifdef DEBUG
  printf("comb delays: %li,%li,%li,%li  %li,%li,%li,%li\n",
        COMB_L_DELAY[0],COMB_L_DELAY[1],COMB_L_DELAY[2],COMB_L_DELAY[3],
	COMB_R_DELAY[0],COMB_R_DELAY[1],COMB_R_DELAY[2],COMB_R_DELAY[3]);
 #endif
 if ((COMB_R_DELAY[0]>MAXDELAY)||(COMB_R_DELAY[1]>MAXDELAY)||
     (COMB_R_DELAY[2]>MAXDELAY)||(COMB_R_DELAY[3]>MAXDELAY)||
     (COMB_L_DELAY[0]>MAXDELAY)||(COMB_L_DELAY[1]>MAXDELAY)||
     (COMB_L_DELAY[2]>MAXDELAY)||(COMB_L_DELAY[3]>MAXDELAY)) {
       printf("Arrays not large enough! Increase MAXDELAY.\n"); exit(1); }
}

/* Filter functions */
double allpass1(double sample,int rl) {
double temp;

 if (rl) {
  temp=lallpassmem1 - sample;
  lallpassmem1=0.70710678 * (sample + lallpassmem1); 
  return temp;
 } else {
  temp=rallpassmem1 - sample;
  rallpassmem1=0.70710678 * (sample + rallpassmem1); 
  return temp;
 }
}

double allpass2(double sample,int rl) {
double temp;

 if (rl) {
  temp=lallpassmem2 - sample;
  lallpassmem2=0.70710678 * (sample + lallpassmem2); 
  return temp;
 } else {
  temp=rallpassmem2 - sample;
  rallpassmem2=0.70710678 * (sample + rallpassmem2); 
  return temp;
 }
}


double comb(double sample,long combid,int rl) {
double temp;
double *memtemp=NULL;

 if (rl == 0) {
  if (combid == 0) memtemp=lmem1;
  if (combid == 1) memtemp=lmem2;
  if (combid == 2) memtemp=lmem3;
  if (combid == 3) memtemp=lmem4;
 } else {
  if (combid == 0) memtemp=rmem1;
  if (combid == 1) memtemp=rmem2;
  if (combid == 2) memtemp=rmem3;
  if (combid == 3) memtemp=rmem4;
 }

 if (rl == 0) {
  memtemp[lstep[combid]]=sample + 
	(COMB_L_GAIN[combid] * memtemp[((lstep[combid] + 1) % COMB_L_DELAY[combid])]);
  temp= memtemp[((lstep[combid] + 1) % COMB_L_DELAY[combid])];
  lstep[combid]++;
  if (lstep[combid] >= COMB_L_DELAY[combid]) lstep[combid]=0;
  return temp;
 } else {
  memtemp[rstep[combid]]=sample + 
	(COMB_R_GAIN[combid] * memtemp[((rstep[combid] + 1) % COMB_R_DELAY[combid])]);
  temp= memtemp[((rstep[combid] + 1) % COMB_R_DELAY[combid])];
  rstep[combid]++;
  if (rstep[combid] >= COMB_R_DELAY[combid]) rstep[combid]=0;
  return temp;
 }
}

/* Put the filters together here */
double reverb(double sample,int rl) {

//return (comb(sample,0,rl) +comb(sample,3,rl))/2.0;

  return ((1.0 - AMMOUNT) * sample) + (AMMOUNT * allpass2(
		allpass1(
		  (comb(sample,0,rl) + comb(sample,1,rl) + comb(sample,2,rl) + comb(sample,3,rl)) / 4
		,rl)
	,rl));
}


/* Main sampling/playback loop */
int init_reverb() {
   //int i,len;
   char samprate[255]="44100\0";
   char fbdist[255]="20\0";
   char decay[255]="2.000\0";
   //long c;

/* Convert args */
 if (sscanf(samprate,"%li",&sampling) == 0) {printf("Bad samprate arg.\n");exit(1);}
 if (sscanf(fbdist,"%i",&fbper) == 0) {printf("Bad fbdist arg.\n");exit(1);}
 if (sscanf(decay,"%f",&decayamt) == 0) {printf("Bad decay arg.\n");exit(1);}

 if (sampling < 4000) {printf("Bad samprate value.\n");exit(1);}
 if ((fbper > 100) || (fbper < 0)) {printf("Bad fbdist value.\n");exit(1);}
 if (decayamt < 0) {printf("Bad decay value.\n");exit(1);}

 printf("\nUsing these parameters: samprate=%ld fbdist=%d decay=%f\n\n",sampling,fbper,decayamt);

 initdelays();
 return 0;
}

bool reverb_func(void *arg, void *data, int size)
{
	// This is an optimization hack
	CorePlayer *p = (CorePlayer *)arg;
	if (!p->IsActive())
		return true;
	// End hack	

  	short *buffer = (short *)data;
 	long c;
	for (int i=0; i < size/2; i++) {
		c = buffer[i];
		long r;
		if (i % 2) 
			r = (long)(reverb(c, 1));		
		else
			r = (long)(reverb(c, 0));
		if (r > 32767) r = 32767;
		else
		if (r < -32768) r = -32768;
		buffer[i] = (short)r;
	}
	return true;
}
