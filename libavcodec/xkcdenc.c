/*
 * BMP image format encoder
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
#include "xkcd.h"
#include "internal.h"

static const uint32_t monoblack_pal[] = { 0x000000, 0xFFFFFF };
static const uint32_t rgb565_masks[]  = { 0xF800, 0x07E0, 0x001F };
static const uint32_t rgb444_masks[]  = { 0x0F00, 0x00F0, 0x000F };

static av_cold int xkcd_encode_init(AVCodecContext *avctx){
    switch (avctx->pix_fmt) {
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_BGR24:
    case AV_PIX_FMT_RGB555:
    case AV_PIX_FMT_RGB565:
    case AV_PIX_FMT_RGB444:
    case AV_PIX_FMT_RGB8:
    case AV_PIX_FMT_BGR8:
    case AV_PIX_FMT_RGB4_BYTE:
    case AV_PIX_FMT_BGR4_BYTE:
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_PAL8:
        avctx->bits_per_coded_sample = 8;
        break;
    case AV_PIX_FMT_MONOBLACK:
        avctx->bits_per_coded_sample = 1;
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
    const AVFrame * const p = pict;	/* Actual image data */

	/* hsize = header size */
    int n_bytes_image, n_bytes_per_row, n_bytes, i, hsize, ret;

    const uint32_t *pal = NULL;	/* pallete entries */
    uint32_t palette256[256];

	/* pad_bytes_per_row = bytes of null to fill in at the end of a row of image data */
    int pad_bytes_per_row, pal_entries = 0;

	/* Number of bits per pixel */
    int bit_count = avctx->bits_per_coded_sample;

	/* ptr = data to be buffered, buf = buffer to write to */
    uint8_t *ptr, *buf;

    avctx->coded_frame->pict_type = AV_PICTURE_TYPE_I;
    avctx->coded_frame->key_frame = 1;
    switch (avctx->pix_fmt) {
    case AV_PIX_FMT_RGB444:
    case AV_PIX_FMT_RGB565:
    case AV_PIX_FMT_RGB8:
    case AV_PIX_FMT_BGR8:
    case AV_PIX_FMT_RGB4_BYTE:
    case AV_PIX_FMT_BGR4_BYTE:
    case AV_PIX_FMT_GRAY8:
        av_assert1(bit_count == 8);
        avpriv_set_systematic_pal2(palette256, avctx->pix_fmt);
        pal = palette256;
        break;
    case AV_PIX_FMT_PAL8:
    case AV_PIX_FMT_MONOBLACK:
        pal = monoblack_pal;
        break;
    }
    if (pal && !pal_entries) pal_entries = 1 << bit_count;

	/* Number of bytes of image data in a row */
	/* (width in pixels * bits per pixel) / 8 to put it in bytes.
	Add 7 bits to the width in bits to make sure to have enough
	bytes of storage when we divide (making sure when it truncates
	in division, it doesn't get rid of what we need) */
    n_bytes_per_row = ((int64_t)avctx->width * (int64_t)bit_count + 7LL) >> 3LL;

	/* Bytes at the end of a row that are 'crossed out' */
	/* Take the remainder from the above bytes and fill in with
	padding by looking at the last two bits after 4 - n_bytes_per_row.*/
    pad_bytes_per_row = (4 - n_bytes_per_row) & 3;

	/* Total bytes in image */
    n_bytes_image = avctx->height * (n_bytes_per_row + pad_bytes_per_row);

    // STRUCTURE.field refer to the MSVC documentation for BITMAPFILEHEADER
    // and related pages.
#define SIZE_XKCDFILEHEADER 14
    hsize = SIZE_XKCDFILEHEADER;

	/* Number of bytes in the entire file */
    n_bytes = n_bytes_image + hsize;

    if ((ret = ff_alloc_packet2(avctx, pkt, n_bytes)) < 0)
        return ret;
    buf = pkt->data;

	/* Start building the header */
    bytestream_put_byte(&buf, 'X');                   // Filetype
    bytestream_put_byte(&buf, 'K');                   // Filetype
    bytestream_put_byte(&buf, 'C');                   // Filetype
    bytestream_put_byte(&buf, 'D');                   // Filetype
    bytestream_put_le32(&buf, n_bytes);               // Size of entire file
    bytestream_put_le16(&buf, avctx->width);          // Width of image in pixels
    bytestream_put_le16(&buf, avctx->height);         // Height of image in pixels
    bytestream_put_le16(&buf, bit_count);             // Bits per pixel


    // BMP files are bottom-to-top so we start from the end...
    ptr = p->data[0];
    /*buf = pkt->data + hsize;*/

	/* Write the image */

    for(i = 0; i < avctx->height; i++) {
		/* Write line to buffer */
		memcpy(buf, ptr, n_bytes_per_row);

		/* Point buffer to the end of the data and start of the padding */
        buf += n_bytes_per_row;

		/* Null out the array which creates padding */
        memset(buf, 0, pad_bytes_per_row);

		/* Point buffer to the end of the padding and start of the new data */
        buf += pad_bytes_per_row;

		/* Now point to next row */
        ptr += p->linesize[0];
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

AVCodec ff_xkcd_encoder = {
    .name           = "xkcd",
    .long_name      = NULL_IF_CONFIG_SMALL("XKCD (eXample of a Kinetic Coder/Decoder) file"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_XKCD,
    .init           = xkcd_encode_init,
    .encode2        = xkcd_encode_frame,
    .close          = xkcd_encode_close,
    .pix_fmts       = (const enum AVPixelFormat[]){
        AV_PIX_FMT_BGRA, AV_PIX_FMT_BGR24,
        AV_PIX_FMT_RGB565, AV_PIX_FMT_RGB555, AV_PIX_FMT_RGB444,
        AV_PIX_FMT_RGB8, AV_PIX_FMT_BGR8, AV_PIX_FMT_RGB4_BYTE, AV_PIX_FMT_BGR4_BYTE, AV_PIX_FMT_GRAY8, AV_PIX_FMT_PAL8,
        AV_PIX_FMT_MONOBLACK,
        AV_PIX_FMT_NONE
    },
};

