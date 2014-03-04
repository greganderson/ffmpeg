/*
 * XKCD image format decoder
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

#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"
#include "xkcd.h"

int getEntry(int[], int, int);
void generateColors(int[], int); 

static int xkcd_decode_frame(AVCodecContext *avctx,
                            void *data, int *got_frame,
                            AVPacket *avpkt)
{
    const uint8_t *buf = avpkt->data; /* Buffer with data */
    int buf_size       = avpkt->size; /* Size of file we are decoding */
    AVFrame *picture   = data;
    unsigned int filesize, headersize; /* File size, header size */
    unsigned int depth;	/* bits per pixel */
    int i, n, linesize, ret, compressed, r, g, b;

	int red[1 << 3], green[1 << 3], blue[1 << 2];

    uint8_t *ptr;

	/* Check initial header size */
    if (buf_size < 14) {
        av_log(avctx, AV_LOG_ERROR, "buf size too small (%d)\n", buf_size);
        return AVERROR_INVALIDDATA;
    }
	/* Make sure header contains correct filetype information */
    if (bytestream_get_byte(&buf) != 'X' ||
        bytestream_get_byte(&buf) != 'K' ||
		bytestream_get_byte(&buf) != 'C' ||
		bytestream_get_byte(&buf) != 'D') {
        av_log(avctx, AV_LOG_ERROR, "illegal filetype information in header\n");
        return AVERROR_INVALIDDATA;
    }

	/* Check file size */
    filesize = bytestream_get_le32(&buf);
    if (buf_size < filesize) {
        av_log(avctx, AV_LOG_ERROR, "not enough data (%d < %d), trying to decode anyway\n",
               buf_size, filesize);
        filesize = buf_size;
    }
	/* Set header size to include the number of bytes our header will use */
    headersize  = 16;

	/* Check to make sure the file size is larger than the header size */
    if (filesize <= headersize) {
        av_log(avctx, AV_LOG_ERROR, "declared file size is less than header size (%d < %d)\n",
               filesize, headersize);
        return AVERROR_INVALIDDATA;
    }

	/* Get width and height of image (in pixels) */
    avctx->width  = bytestream_get_le16(&buf);
	avctx->height = bytestream_get_le16(&buf);

	/* Bits per pixel */
	depth = bytestream_get_le16(&buf);

	/* A flag for whether the file was compressed or not */
		compressed = bytestream_get_le16(&buf);   
	
	if(compressed == XKCD_RGB24){
		/* Set the color format */
		avctx->pix_fmt = AV_PIX_FMT_RGB24;
	}
	else {
		/* Set the color format */
		avctx->pix_fmt = AV_PIX_FMT_RGB8;
	}

	if ((ret = ff_get_buffer(avctx, picture, 0)) < 0)
		return ret;

	picture->pict_type = AV_PICTURE_TYPE_I;
	picture->key_frame = 1;

	buf += headersize; /* Point buf at the start of the pixel array */

	/* Line size in file multiple of 4 */
	n = ((avctx->width * depth + 31) / 8) & ~3;

	/* Set the pointer to the top left part of the image */
	ptr      = picture->data[0];		/* Actual pointer to the image data */
	linesize = picture->linesize[0];	/* Size of the line in bytes */

	if(compressed == XKCD_RGB24){
		/* Initialize rgb */
		r = 3;
		g = 3;
		b = 2;

		generateColors(red, r);
		generateColors(green, g);
		generateColors(blue, b);


		for(i = 0; i  < sizeof(picture->data); i++){
		}
	}
	else {
		/* Decode the actual image */
		for (i = 0; i < avctx->height; i++) {
			memcpy(ptr, buf, n);
			buf += n;
			ptr += linesize;
		}
	}

    *got_frame = 1;
    return buf_size;
}

AVCodec ff_xkcd_decoder = {
    .name           = "xkcd",
    .long_name      = NULL_IF_CONFIG_SMALL("XKCD (eXample of a Kinetic Coder/Decoder) file"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_XKCD,
    .decode         = xkcd_decode_frame,
    .capabilities   = CODEC_CAP_DR1,
};
