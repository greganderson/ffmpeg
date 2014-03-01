/*
 * BMP image format decoder
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
#include "xkcd.h"
#include "internal.h"
#include "msrledec.h"

static int xkcd_decode_frame(AVCodecContext *avctx,
                            void *data, int *got_frame,
                            AVPacket *avpkt)
{
    const uint8_t *buf = avpkt->data; /* Buffer with data */
    int buf_size       = avpkt->size; /* Size of file we are decoding */
    AVFrame *p         = data;
    unsigned int fsize, hsize; /* File size, header size */
    unsigned int depth;
    int i, n, linesize, ret;
    uint8_t *ptr;
    const uint8_t *buf0 = buf;

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
    fsize = bytestream_get_le32(&buf);
    if (buf_size < fsize) {
        av_log(avctx, AV_LOG_ERROR, "not enough data (%d < %d), trying to decode anyway\n",
               buf_size, fsize);
        fsize = buf_size;
    }

    hsize  = 14;

	/* Check to make sure the file size is larger than the header size */
    if (fsize <= hsize) {
        av_log(avctx, AV_LOG_ERROR, "declared file size is less than header size (%d < %d)\n",
               fsize, hsize);
        return AVERROR_INVALIDDATA;
    }

	/* Get width and height of image (in pixels) */
    avctx->width  = bytestream_get_le16(&buf);
	avctx->height = bytestream_get_le16(&buf);

	/* Bits per pixel */
	depth = bytestream_get_le16(&buf);

	avctx->pix_fmt = AV_PIX_FMT_PAL8;
    
	if ((ret = ff_get_buffer(avctx, p, 0)) < 0)
        return ret;

    p->pict_type = AV_PICTURE_TYPE_I;
    p->key_frame = 1;

    buf   = buf0 + hsize; /* Point buf at the start of the pixel array */

    /* Line size in file multiple of 4 (without padding)*/
    n = ((avctx->width * depth + 31) / 8) & ~3;

    // RLE may skip decoding some picture areas, so blank picture before decoding
	memset(p->data[0], 0, avctx->height * p->linesize[0]);

	/* Set the pointer to the top left part of the image */
	ptr      = p->data[0];	/* Actual pointer to the image data */
	linesize = p->linesize[0];

	/* Stores how many colors there are, determined by the number of bits per pixel.
	   For example, 8 bits = 256 colors, 4 bits = 16 colors*/
	/*int colors = 1 << depth; */

	/* Null out the buffer to hold the image */
	memset(p->data[1], 0, 1024);


	av_log(avctx, AV_LOG_ERROR, "Got here linesize=%d\n", linesize);
	/* Decode the actual image */
	for (i = 0; i < avctx->height; i++) {
		/* Segfault on the 3rd iteration of the loop, loop should proceed through 233 iterations */	
		memcpy(ptr, buf, n);
		buf += n;
		ptr += linesize;
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
