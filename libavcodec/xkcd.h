/*
 * internals for XKCD codecs
 * Copyright (c) 2005 Mans Rullgard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_XKCD_H
#define AVCODEC_XKCD_H

#include "avcodec.h"
int getEntry(int table[], int color);
void generateColors(int arr[], int bits);

typedef enum {
    XKCD_RGB         =0,
    XKCD_RLE8        =1,
    XKCD_RLE4        =2,
    XKCD_BITFIELDS   =3,
} BiCompression;

/* Returns the entry in the specific color table */
int getEntry(int table[], int color) {
	int i = 0;
	while (color > table[i])
		i++;
	return i;
}

/* Generates a table of colors with then number of available bits supplied */
void generateColors(int arr[], int bits) {
	int val, i;
	val = 255 / ((1 << bits)-1);
	for (i = 0; i < (1 << bits); i++)
		arr[i] = val * i;
}

#endif /* AVCODEC_XKCD_H */
