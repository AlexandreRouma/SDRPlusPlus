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

#ifndef __WAVEHDR_H
#define __WAVEHDR_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#pragma pack(push)
#pragma pack(1)

typedef struct
{
	char		ID[4];
	uint32_t	size;
} chunk_hdr;

typedef struct
{
	uint16_t	wYear;			/* 1601 through 30827 */
	uint16_t	wMonth;			/* 1..12 */
	uint16_t	wDayOfWeek;		/* 0 .. 6: 0 == Sunday, .., 6 == Saturday */
	uint16_t	wDay;			/* 1 .. 31 */
	uint16_t	wHour;			/* 0 .. 23 */
	uint16_t	wMinute;		/* 0 .. 59 */
	uint16_t	wSecond;		/* 0 .. 59 */
	uint16_t	wMilliseconds;	/* 0 .. 999 */
} Wind_SystemTime;


typedef struct
{
	/* RIFF header */
	chunk_hdr	hdr;		/* ID == "RIFF" string, size == full filesize - 8 bytes (maybe with some byte missing...) */
	char		waveID[4];	/* "WAVE" string */
} riff_chunk;

typedef struct
{
	/* FMT header */
	chunk_hdr	hdr;		/* ID == "fmt " */
	int16_t		wFormatTag;
	int16_t		nChannels;
	int32_t		nSamplesPerSec;
	int32_t		nAvgBytesPerSec;
	int16_t		nBlockAlign;
	int16_t		nBitsPerSample;
} fmt_chunk;

typedef struct
{
	/* auxi header - used by SpectraVue / rfspace / HDSDR / ELAD FDM .. */
	chunk_hdr	hdr;		/* ="auxi" (chunk rfspace) */
	Wind_SystemTime StartTime;
	Wind_SystemTime StopTime;
	uint32_t	centerFreq;		/* receiver center frequency */
	uint32_t	ADsamplerate;	/* A/D sample frequency before downsampling */
	uint32_t	IFFrequency;	/* IF freq if an external down converter is used */
	uint32_t	Bandwidth;		/* displayable BW if you want to limit the display to less than Nyquist band */
	int32_t		IQOffset;		/* DC offset of the I and Q channels in 1/1000's of a count */
	int32_t		Unused2;
	int32_t		Unused3;
	int32_t		Unused4;
	int32_t		Unused5;
} auxi_chunk;

typedef struct
{
	/* DATA header */
	chunk_hdr	hdr;		/* ="data" */
} data_chunk;

typedef struct
{
	riff_chunk r;
	fmt_chunk  f;
	auxi_chunk a;
	data_chunk d;
} waveFileHeader;

#pragma pack(pop)

#endif /* __WAVEHDR_H */

// vim: tabstop=8:softtabstop=8:shiftwidth=8:noexpandtab

