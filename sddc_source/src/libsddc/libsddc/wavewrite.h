/*
 * Copyright (C) 2019 by Hayati Ayguen <h_ayguen@web.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __WAVEWRITE_H
#define __WAVEWRITE_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int	waveHdrStarted;

/*!
 * helper functions to write and finalize wave headers
 *   with compatibility to some SDR programs - showing frequency:
 * raw sample data still have to be written by caller to FILE*.
 * call waveWriteHeader() before writing anything to to file
 * and call waveFinalizeHeader() afterwards,
 * stdout/stderr can't be used, because seek to begin isn't possible.
 * 
 */

void waveWriteHeader(unsigned samplerate, unsigned freq, int bitsPerSample, int numChannels, FILE * f);

/* waveWriteFrames() writes (numFrames * numChannels) samples
 * waveWriteSamples()
 * both return 0, when no errors occured
 */
int  waveWriteFrames(FILE* f,  void * vpData, size_t numFrames, int needCleanData);
int  waveWriteSamples(FILE* f,  void * vpData, size_t numSamples, int needCleanData);  /* returns 0, when no errors occured */
void waveSetStartTime(time_t t, double fraction);
int  waveFinalizeHeader(FILE * f);      /* returns 0, when no errors occured */

#ifdef __cplusplus
}
#endif

#endif /*__WAVEWRITE_H*/
