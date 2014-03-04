/*
 * XKCD image format encoder
 * Copyright (c) 2006, 2007 Michel Bardiaux
 * Copyright (c) 2009 Daniel Verkamp <daniel at drv.nu>
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

#include "libavutil/imgutils.h"
#include "libavutil/avassert.h"
#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"
#include "xkcd.h"

int getEntry(int[], int, int);
void generateColors(int[], int);

static av_cold int xkcd_encode_init(AVCodecContext *avctx){
    switch (avctx->pix_fmt) {
	case AV_PIX_FMT_RGB24:
		avctx->bits_per_coded_sample = 24;
		break;
    case AV_PIX_FMT_RGB8:
        avctx->bits_per_coded_sample = 8;
        break;
    default:
        av_log(avctx, AV_LOG_INFO, "unsupported pixel format\n");
        return AVERROR(EINVAL);
    }

    avctx->coded_frame = av_frame_alloc();
    if (!avctx->coded_frame)
        return AVERROR(ENOMEM);
	
    return 0;
}

static int xkcd_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                            const AVFrame *pict, int *got_packet)
{
    const AVFrame * const picture = pict;	/* Actual image data */

	/* header_size = header size */
    int i, j, bytes_in_image, bytes_per_row, total_bytes, header_size, ret;

	/* pad_bytes_per_row = bytes of null to fill in at the end of a row of image data */
    int pad_bytes_per_row = 0;

	/* Number of bits per pixel */
    int bit_count = avctx->bits_per_coded_sample;

	/* buffer_data = data to be buffered, buf = buffer to write to */
    uint8_t *buffer_data, *buffer, *buffer_start;

	int compression, r, g, b, rbits, gbits, bbits, sum = 0;

	/* Color tables: color[2**NumBitsForColor]*/
	int red[1 << 3];
	int green[1 << 3];
	int blue[1 << 2];
	rbits = 3;
	gbits = 3;
	bbits = 2;

	if (avctx->bits_per_coded_sample == 24)
		compression = XKCD_RGB24;
	else
		compression = XKCD_RGB8;

    avctx->coded_frame->pict_type = AV_PICTURE_TYPE_I;
    avctx->coded_frame->key_frame = 1;

	/* Number of bytes of image data in a row */
	/* (width in pixels * bits per pixel) / 8 to put it in bytes.
	Add 7 bits to the width in bits to make sure to have enough
	bytes of storage when we divide (making sure when it truncates
	in division, it doesn't get rid of what we need) */
    bytes_per_row = ((int64_t)avctx->width * (int64_t)bit_count + 7LL) >> 3LL;

	/* Bytes at the end of a row that are 'crossed out' */
	/* Take the remainder from the above bytes and fill in with
	padding by looking at the last two bits after 4 - bytes_per_row.*/
    pad_bytes_per_row = (4 - bytes_per_row) & 3;

	/* Total bytes in image */
    bytes_in_image = avctx->height * (bytes_per_row + pad_bytes_per_row);

	header_size = 16;

	/* Number of bytes in the entire file */
	if (avctx->bits_per_coded_sample == 24)
		total_bytes = (bytes_in_image / 3) + header_size;
	else
		total_bytes = bytes_in_image + header_size;
		

    if ((ret = ff_alloc_packet2(avctx, pkt, total_bytes)) < 0)
        return ret;

    buffer_start = buffer = pkt->data;

	/* Start building the header */
    bytestream_put_byte(&buffer, 'X');                   // Filetype
    bytestream_put_byte(&buffer, 'K');                   // Filetype
    bytestream_put_byte(&buffer, 'C');                   // Filetype
    bytestream_put_byte(&buffer, 'D');                   // Filetype
    bytestream_put_le32(&buffer, total_bytes);           // Size of entire file
    bytestream_put_le16(&buffer, avctx->width);          // Width of image in pixels
    bytestream_put_le16(&buffer, avctx->height);         // Height of image in pixels
    bytestream_put_le16(&buffer, bit_count);             // Bits per pixel
    bytestream_put_le16(&buffer, compression);           // Compression type

    // Start buffer at the correct position in memory
    buffer_data = picture->data[0];

	if (compression == XKCD_RGB24) {
		/* Generate the color tables */
		generateColors(red, rbits);
		generateColors(green, gbits);
		generateColors(blue, bbits);

		for (i = 0; i < avctx->height; i++) {
			/* Loop through row of pixels, moving 3 bytes at a time (24 bit color) */
			for (j = 0; j < bytes_per_row; j+=3) {
				/* Get rgb values */
				r = getEntry(red, buffer_data[j+0], rbits);
				g = getEntry(green, buffer_data[j+1], gbits);
				b = getEntry(blue, buffer_data[j+2], bbits);

				sum = (r << (8-rbits)) + (g << (8-rbits-gbits)) + (b << (8-rbits-gbits-bbits));

				/* Store the sum */
				bytestream_put_byte(&buffer, sum);
			}
			/* Null out the array which creates padding */
			memset(buffer, 0, pad_bytes_per_row);

			/* Point buffer to the end of the padding and start of the new data */
			buffer += pad_bytes_per_row;

			/* Now point to next row */
			buffer_data += picture->linesize[0];
			//buffer_data += bytes_per_row;
		}

		/* Set the filesize */
		pkt->size = buffer - buffer_start;
	}
	else if (compression == XKCD_RGB8) {
		/* Write the image */
		for(i = 0; i < avctx->height; i++) {
			/* Write line to buffer */
			memcpy(buffer, buffer_data, bytes_per_row);

			/* Point buffer to the end of the data and start of the padding */
			buffer += bytes_per_row;

			/* Null out the array which creates padding */
			memset(buffer, 0, pad_bytes_per_row);

			/* Point buffer to the end of the padding and start of the new data */
			buffer += pad_bytes_per_row;

			/* Now point to next row */
			buffer_data += picture->linesize[0];
		}
	}

    pkt->flags |= AV_PKT_FLAG_KEY;
    *got_packet = 1;

    return 0;
}

static av_cold int xkcd_encode_close(AVCodecContext *avctx)
{
    av_frame_free(&avctx->coded_frame);
    return 0;
}

/* Returns the entry in the specific color table */
int getEntry(int table[], int color, int bits) {
	int i;
	for (i = 0; i < (1 << bits)-1; i++)
		if (color <= table[i])
			return i;

	return i;
}

/* Generates a table of colors with then number of available bits supplied */
void generateColors(int arr[], int bits) {
	int val, i;
	val = 255 / ((1 << bits)-1);
	for (i = 0; i < (1 << bits); i++)
		arr[i] = val * i;
}

AVCodec ff_xkcd_encoder = {
    .name           = "xkcd",
    .long_name      = NULL_IF_CONFIG_SMALL("XKCD (eXample of a Kinetic Coder/Decoder) file"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_XKCD,
    .init           = xkcd_encode_init,
    .encode2        = xkcd_encode_frame,
    .close          = xkcd_encode_close,
    .pix_fmts       = (const enum AVPixelFormat[]){
        AV_PIX_FMT_RGB8, AV_PIX_FMT_RGB24,
		AV_PIX_FMT_NONE
    },
};

