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

#include "wavewrite.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#else
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#define _USE_MATH_DEFINES

#include <stdint.h> // portable: uint64_t   MSVC: __int64 

int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970 
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}
#endif

#include <math.h>

#include "wavehdr.h"

static waveFileHeader waveHdr;

static uint32_t	waveDataSize = 0;
int	waveHdrStarted = 0;


static void waveSetCurrTime(Wind_SystemTime *p)
{
	struct timeval tv;
	struct tm t;

	gettimeofday(&tv, NULL);
	p->wMilliseconds = tv.tv_usec / 1000;

#ifdef _WIN32
	t = *gmtime(&tv.tv_sec);
#else
	gmtime_r(&tv.tv_sec, &t);
#endif

	p->wYear = t.tm_year + 1900;	/* 1601 through 30827 */
	p->wMonth = t.tm_mon + 1;		/* 1..12 */
	p->wDayOfWeek = t.tm_wday;		/* 0 .. 6: 0 == Sunday, .., 6 == Saturday */
	p->wDay = t.tm_mday;			/* 1 .. 31 */
	p->wHour = t.tm_hour;			/* 0 .. 23 */
	p->wMinute = t.tm_min;			/* 0 .. 59 */
	p->wSecond = t.tm_sec;			/* 0 .. 59 */
}

static void waveSetStartTimeInt(time_t tim, double fraction, Wind_SystemTime *p)
{
	struct tm t = *gmtime( &tim );
	p->wYear = t.tm_year + 1900;	/* 1601 through 30827 */
	p->wMonth = t.tm_mon + 1;		/* 1..12 */
	p->wDayOfWeek = t.tm_wday;		/* 0 .. 6: 0 == Sunday, .., 6 == Saturday */
	p->wDay = t.tm_mday;			/* 1 .. 31 */
	p->wHour = t.tm_hour;			/* 0 .. 23 */
	p->wMinute = t.tm_min;			/* 0 .. 59 */
	p->wSecond = t.tm_sec;			/* 0 .. 59 */
	p->wMilliseconds = (int)( fraction * 1000.0 );
	if (p->wMilliseconds >= 1000)
		p->wMilliseconds = 999;
}

void waveSetStartTime(time_t tim, double fraction)
{
	waveSetStartTimeInt(tim, fraction, &waveHdr.a.StartTime );
	waveHdr.a.StopTime = waveHdr.a.StartTime;		/* to fix */
}


void wavePrepareHeader(unsigned samplerate, unsigned freq, int bitsPerSample, int numChannels)
{
	int	bytesPerSample = bitsPerSample / 8;
	int bytesPerFrame = bytesPerSample * numChannels;

	memcpy( waveHdr.r.hdr.ID, "RIFF", 4 );
	waveHdr.r.hdr.size = sizeof(waveFileHeader) - 8;		/* to fix */
	memcpy( waveHdr.r.waveID, "WAVE", 4 );

	memcpy( waveHdr.f.hdr.ID, "fmt ", 4 );
	waveHdr.f.hdr.size = 16;
	waveHdr.f.wFormatTag = 1;					/* PCM */
	waveHdr.f.nChannels = numChannels;		/* I and Q channels */
	waveHdr.f.nSamplesPerSec = samplerate;
	waveHdr.f.nAvgBytesPerSec = samplerate * bytesPerFrame;
	waveHdr.f.nBlockAlign = waveHdr.f.nChannels;
	waveHdr.f.nBitsPerSample = bitsPerSample;

	memcpy( waveHdr.a.hdr.ID, "auxi", 4 );
	waveHdr.a.hdr.size = 2 * sizeof(Wind_SystemTime) + 9 * sizeof(int32_t);  /* = 2 * 16 + 9 * 4 = 68 */
	waveSetCurrTime( &waveHdr.a.StartTime );
	waveHdr.a.StopTime = waveHdr.a.StartTime;		/* to fix */
	waveHdr.a.centerFreq = freq;
	waveHdr.a.ADsamplerate = samplerate;
	waveHdr.a.IFFrequency = 0;
	waveHdr.a.Bandwidth = 0;
	waveHdr.a.IQOffset = 0;
	waveHdr.a.Unused2 = 0;
	waveHdr.a.Unused3 = 0;
	waveHdr.a.Unused4 = 0;
	waveHdr.a.Unused5 = 0;

	memcpy( waveHdr.d.hdr.ID, "data", 4 );
	waveHdr.d.hdr.size = 0;		/* to fix later */
	waveDataSize = 0;
}

void waveWriteHeader(unsigned samplerate, unsigned freq, int bitsPerSample, int numChannels, FILE * f)
{
	if (f != stdout) {
		assert( !waveHdrStarted );
		wavePrepareHeader(samplerate, freq, bitsPerSample, numChannels);
		fwrite(&waveHdr, sizeof(waveFileHeader), 1, f);
		waveHdrStarted = 1;
	}
}

int  waveWriteSamples(FILE* f,  void * vpData, size_t numSamples, int needCleanData)
{
	size_t nw;
	switch (waveHdr.f.nBitsPerSample)
	{
	case 0:
	default:
		return 1;
	case 8:
		/* no endian conversion needed for single bytes */
		nw = fwrite(vpData, sizeof(uint8_t), numSamples, f);
		waveDataSize += sizeof(uint8_t) * numSamples;
		return (nw == numSamples) ? 0 : 1;
	case 16:
		/* TODO: endian conversion needed */
		nw = fwrite(vpData, sizeof(int16_t), numSamples, f);
		waveDataSize += sizeof(int16_t) * numSamples;
		if ( needCleanData )
		{
			/* TODO: convert back endianness */
		}
		return (nw == numSamples) ? 0 : 1;
	}
}

int  waveWriteFrames(FILE* f,  void * vpData, size_t numFrames, int needCleanData)
{
	size_t nw;
	switch (waveHdr.f.nBitsPerSample)
	{
	case 0:
	default:
		return 1;
	case 8:
		/* no endian conversion needed for single bytes */
		nw = fwrite(vpData, waveHdr.f.nChannels * sizeof(uint8_t), numFrames, f);
		waveDataSize += waveHdr.f.nChannels * sizeof(uint8_t) * numFrames;
		return (nw == numFrames) ? 0 : 1;
	case 16:
		/* TODO: endian conversion needed */
		nw = fwrite(vpData, waveHdr.f.nChannels * sizeof(int16_t), numFrames, f);
		waveDataSize += waveHdr.f.nChannels * sizeof(int16_t) * numFrames;
		if ( needCleanData )
		{
			/* TODO: convert back endianness */
		}
		return (nw == numFrames) ? 0 : 1;
	}
}


int  waveFinalizeHeader(FILE * f)
{
	if (f != stdout) {
		assert( waveHdrStarted );
		waveSetCurrTime( &waveHdr.a.StopTime );
		waveHdr.d.hdr.size = waveDataSize;
		waveHdr.r.hdr.size += waveDataSize;
		/* fprintf(stderr, "waveFinalizeHeader(): datasize = %d\n", waveHdr.dataSize); */
		waveHdrStarted = 0;
		if ( fseek(f, 0, SEEK_SET) )
			return 1;
		if ( 1 != fwrite(&waveHdr, sizeof(waveFileHeader), 1, f) )
			return 1;
		/* fprintf(stderr, "waveFinalizeHeader(): success writing header\n"); */
		return 0;
	}
	return 1;
}

// vim: tabstop=8:softtabstop=8:shiftwidth=8:noexpandtab
