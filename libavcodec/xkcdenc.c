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

static av_cold int bmp_encode_init(AVCodecContext *avctx){
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

static int bmp_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                            const AVFrame *pict, int *got_packet)
{
    const AVFrame * const p = pict;	/* Actual image data */

	/* hsize = header size */
    int n_bytes_image, n_bytes_per_row, n_bytes, i, n, hsize, ret;

    const uint32_t *pal = NULL;	/* pallete entries */
    uint32_t palette256[256];

	/* pad_bytes_per_row = bytes of null to fill in at the end of a row of image data */
    int pad_bytes_per_row, pal_entries = 0, compression = XKCD_RGB;

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
    n_bytes_per_row = ((int64_t)avctx->width * (int64_t)bit_count + 7LL) >> 3LL;

	/* Bytes at the end of a row that are 'crossed out' */
    pad_bytes_per_row = (4 - n_bytes_per_row) & 3;

	/* Total bytes in image */
    n_bytes_image = avctx->height * (n_bytes_per_row + pad_bytes_per_row);

    // STRUCTURE.field refer to the MSVC documentation for BITMAPFILEHEADER
    // and related pages.
#define SIZE_XKCDFILEHEADER 17
    hsize = SIZE_XKCDFILEHEADER + (pal_entries << 2);
    n_bytes = n_bytes_image + hsize;
    if ((ret = ff_alloc_packet2(avctx, pkt, n_bytes)) < 0)
        return ret;
    buf = pkt->data;
    bytestream_put_byte(&buf, 'X');                   // BITMAPFILEHEADER.bfType
    bytestream_put_byte(&buf, 'K');                   // do.
    bytestream_put_byte(&buf, 'C');                   // do.
    bytestream_put_byte(&buf, 'D');                   // do.
    bytestream_put_le32(&buf, n_bytes);               // BITMAPFILEHEADER.bfSize
    bytestream_put_le16(&buf, 0);                     // BITMAPFILEHEADER.bfReserved1
    bytestream_put_le16(&buf, 0);                     // BITMAPFILEHEADER.bfReserved2
    bytestream_put_le32(&buf, hsize);                 // BITMAPFILEHEADER.bfOffBits
    bytestream_put_le32(&buf, SIZE_BITMAPINFOHEADER); // BITMAPINFOHEADER.biSize
    bytestream_put_le32(&buf, avctx->width);          // BITMAPINFOHEADER.biWidth
    bytestream_put_le32(&buf, avctx->height);         // BITMAPINFOHEADER.biHeight
    bytestream_put_le16(&buf, 1);                     // BITMAPINFOHEADER.biPlanes
    bytestream_put_le16(&buf, bit_count);             // BITMAPINFOHEADER.biBitCount
    bytestream_put_le32(&buf, compression);           // BITMAPINFOHEADER.biCompression
    bytestream_put_le32(&buf, n_bytes_image);         // BITMAPINFOHEADER.biSizeImage
    bytestream_put_le32(&buf, 0);                     // BITMAPINFOHEADER.biXPelsPerMeter
    bytestream_put_le32(&buf, 0);                     // BITMAPINFOHEADER.biYPelsPerMeter
    bytestream_put_le32(&buf, 0);                     // BITMAPINFOHEADER.biColorUsed
    bytestream_put_le32(&buf, 0);                     // BITMAPINFOHEADER.biColorImportant
    for (i = 0; i < pal_entries; i++)
        bytestream_put_le32(&buf, pal[i] & 0xFFFFFF);
    // BMP files are bottom-to-top so we start from the end...
    ptr = p->data[0] + (avctx->height - 1) * p->linesize[0];
    buf = pkt->data + hsize;

	/* Write the image */

    for(i = 0; i < avctx->height; i++) {
		/* Write line to buffer */
		memcpy(buf, ptr, n_bytes_per_row);

		/* Start buffer at a new line */
        buf += n_bytes_per_row;

		/* Null out the array */
        memset(buf, 0, pad_bytes_per_row);

        buf += pad_bytes_per_row;
        ptr -= p->linesize[0]; // ... and go back
    }

    pkt->flags |= AV_PKT_FLAG_KEY;
    *got_packet = 1;
    return 0;
}

static av_cold int bmp_encode_close(AVCodecContext *avctx)
{
    av_frame_free(&avctx->coded_frame);
    return 0;
}

AVCodec ff_xkcd_encoder = {
    .name           = "xkcd",
    .long_name      = NULL_IF_CONFIG_SMALL("BMP (Windows and OS/2 bitmap)"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_XKCD,
    .init           = bmp_encode_init,
    .encode2        = bmp_encode_frame,
    .close          = bmp_encode_close,
    .pix_fmts       = (const enum AVPixelFormat[]){
        AV_PIX_FMT_BGRA, AV_PIX_FMT_BGR24,
        AV_PIX_FMT_RGB565, AV_PIX_FMT_RGB555, AV_PIX_FMT_RGB444,
        AV_PIX_FMT_RGB8, AV_PIX_FMT_BGR8, AV_PIX_FMT_RGB4_BYTE, AV_PIX_FMT_BGR4_BYTE, AV_PIX_FMT_GRAY8, AV_PIX_FMT_PAL8,
        AV_PIX_FMT_MONOBLACK,
        AV_PIX_FMT_NONE
    },
};

