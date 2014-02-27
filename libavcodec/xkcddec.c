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
    int buf_size       = avpkt->size; /* Size of buffer */
    AVFrame *p         = data;
    unsigned int fsize, hsize; /* File size, header size */
    unsigned int depth;
    int i, j, n, linesize, ret;
    uint32_t rgb[3] = {0};	/* Colors */
    uint8_t *ptr;
    int dsize;	/* Data/image size */
    const uint8_t *buf0 = buf;
    GetByteContext gb;

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

    hsize  = bytestream_get_le32(&buf); /* header size */

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

    buf   = buf0 + hsize;
    dsize = buf_size - hsize;

    /* Line size in file multiple of 4 */
    n = ((avctx->width * depth + 31) / 8) & ~3;

    // RLE may skip decoding some picture areas, so blank picture before decoding
	memset(p->data[0], 0, avctx->height * p->linesize[0]);

	/* Set the pointer to the top left part of the image */
	ptr      = p->data[0];	/* Actual pointer to the image data */
	linesize = -p->linesize[0];

    if (avctx->pix_fmt == AV_PIX_FMT_PAL8) {
        int colors = 1 << depth;

        memset(p->data[1], 0, 1024);

        buf = buf0 + 14 + ihsize; //palette location
        // OS/2 bitmap, 3 bytes per palette entry
        if ((hsize-ihsize-14) < (colors << 2)) {
            if ((hsize-ihsize-14) < colors * 3) {
                av_log(avctx, AV_LOG_ERROR, "palette doesn't fit in packet\n");
                return AVERROR_INVALIDDATA;
            }
            for (i = 0; i < colors; i++)
                ((uint32_t*)p->data[1])[i] = (0xFFU<<24) | bytestream_get_le24(&buf);
        } else {
            for (i = 0; i < colors; i++)
                ((uint32_t*)p->data[1])[i] = 0xFFU << 24 | bytestream_get_le32(&buf);
        }
        buf = buf0 + hsize;
    }

	/* Decode the actual image */
	for (i = 0; i < avctx->height; i++) {
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
