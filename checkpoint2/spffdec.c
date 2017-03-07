/*
 * spff image format decoder, code based on BMP format
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

#include <inttypes.h>

#include "avcodec.h"
#include "bytestream.h"
#include "spff.h"
#include "internal.h"
#include "msrledec.h"

static int spff_decode_frame(AVCodecContext *avctx,
                            void *data, int *got_frame,
                            AVPacket *avpkt)
{
    const uint8_t *buf = avpkt->data;
    int buf_size       = avpkt->size;
    AVFrame *p         = data;
    unsigned int fsize, hsize;
    int width, height;
    unsigned int bit_count;
    BiCompression comp;
    unsigned int ihsize;
    int i, j, n, linesize, ret;
    uint32_t rgb[3] = {0};
    //   uint32_t alpha = 0;
    uint8_t *ptr;
    int dsize;
    const uint8_t *buf0 = buf;
    GetByteContext gb;

     // print out CS 3505 stuff
    static int been_here  = 0;

    if(!been_here)
      av_log(avctx,AV_LOG_INFO, "\n*** CS 3505: Executing in %s and %s***\n*** CS 3505: Modified by To Tang and Minh Pham *** \n ","spff_decode_frame","spffdec.c");
    been_here = 1;

    if (buf_size < 10) {
        av_log(avctx, AV_LOG_ERROR, "buf size too small (%d)\n", buf_size);
        return AVERROR_INVALIDDATA;
    }

    if (bytestream_get_byte(&buf) != 'S' ||
        bytestream_get_byte(&buf) != 'F') {
        av_log(avctx, AV_LOG_ERROR, "bad magic number\n");
        return AVERROR_INVALIDDATA;
    }

    fsize = bytestream_get_le32(&buf);
    if (buf_size < fsize) {
        av_log(avctx, AV_LOG_ERROR, "not enough data (%d < %u), trying to decode anyway\n",
               buf_size, fsize);
        fsize = buf_size;
    }
    if(avctx->width < 0 || avctx->height <0){
       av_log(avctx, AV_LOG_ERROR, "negative width or height\n");
    }
    
    hsize  = bytestream_get_le32(&buf); /* header size */
    ihsize = bytestream_get_le32(&buf); /* more header size */
    if (ihsize + 10LL > hsize) {
        av_log(avctx, AV_LOG_ERROR, "invalid header size %u\n", hsize);
        return AVERROR_INVALIDDATA;
    }

    /* sometimes file size is set to some headers size, set a real size in that case */
    if (fsize == 14 || fsize == ihsize + 10)
        fsize = buf_size - 2;

    if (fsize <= hsize) {
        av_log(avctx, AV_LOG_ERROR,
               "Declared file size is less than header size (%u < %u)\n",
               fsize, hsize);
        return AVERROR_INVALIDDATA;
    }

    width  = bytestream_get_le32(&buf);
    height = bytestream_get_le32(&buf);
    /*
    switch (ihsize) {
    case  40: // windib
    case  56: // windib v3
    case  64: // OS/2 v2
    case 108: // windib v4
    case 124: // windib v5
        width  = bytestream_get_le32(&buf);
        height = bytestream_get_le32(&buf);
        break;
    case  12: // OS/2 v1
        width  = bytestream_get_le16(&buf);
        height = bytestream_get_le16(&buf);
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "unsupported spff file, patch welcome\n");
        return AVERROR_PATCHWELCOME;
    }
    */

    /* planes */
    if (bytestream_get_le16(&buf) != 1) {
        av_log(avctx, AV_LOG_ERROR, "invalid SPFF header\n");
        return AVERROR_INVALIDDATA;
    }

    bit_count = bytestream_get_le16(&buf); // bit_count

    if (ihsize >= 40)
        comp = bytestream_get_le32(&buf);
    else
        comp = SPFF_RGB;

    if (comp != SPFF_RGB && comp != SPFF_BITFIELDS && comp != SPFF_RLE4 &&
        comp != SPFF_RLE8) {
        av_log(avctx, AV_LOG_ERROR, "SPFF coding %d not supported\n", comp);
        return AVERROR_INVALIDDATA;
    }
    /*
    if (comp == SPFF_BITFIELDS) {
        buf += 20;
        rgb[0] = bytestream_get_le32(&buf);
        rgb[1] = bytestream_get_le32(&buf);
        rgb[2] = bytestream_get_le32(&buf);
        if (ihsize > 40)
        alpha = bytestream_get_le32(&buf);
    }
    */
    avctx->width  = width;
    avctx->height = height > 0 ? height : -height;

    avctx->pix_fmt = AV_PIX_FMT_NONE;
    if(bit_count == 24)
      avctx->pix_fmt = AV_PIX_FMT_BGR24;
    else
      {
        av_log(avctx, AV_LOG_ERROR, "depth %u not supported\n", bit_count);
        return AVERROR_INVALIDDATA;
      }
    if (avctx->pix_fmt == AV_PIX_FMT_NONE) {
        av_log(avctx, AV_LOG_ERROR, "unsupported pixel format\n");
        return AVERROR_INVALIDDATA;
    }

    if ((ret = ff_get_buffer(avctx, p, 0)) < 0)
        return ret;
    p->pict_type = AV_PICTURE_TYPE_I;
    p->key_frame = 1;

    buf   = buf0 + hsize;
    dsize = buf_size - hsize;

    /* Line size in file multiple of 4 */
    n = ((avctx->width * bit_count + 31) / 8) & ~3;
    if (n * avctx->height > dsize && comp != SPFF_RLE4 && comp != SPFF_RLE8) {
        n = (avctx->width * bit_count + 7) / 8;
        if (n * avctx->height > dsize) {
            av_log(avctx, AV_LOG_ERROR, "not enough data (%d < %d)\n",
                   dsize, n * avctx->height);
            return AVERROR_INVALIDDATA;
        }
        av_log(avctx, AV_LOG_ERROR, "data size too small, assuming missing line alignment\n");
    }

    // RLE may skip decoding some picture areas, so blank picture before decoding
    /*
    if (comp == SPFF_RLE4 || comp == SPFF_RLE8)
        memset(p->data[0], 0, avctx->height * p->linesize[0]);

    if (height > 0) {
        ptr      = p->data[0] + (avctx->height - 1) * p->linesize[0];
        linesize = -p->linesize[0];
    } else {
        ptr      = p->data[0];
        linesize = p->linesize[0];
    }
    */
    ptr      = p->data[0] + (avctx->height - 1) * p->linesize[0];
    linesize = -p->linesize[0];

    /*
    if (comp == SPFF_RLE4 || comp == SPFF_RLE8) {
        if (comp == SPFF_RLE8 && height < 0) {
            p->data[0]    +=  p->linesize[0] * (avctx->height - 1);
            p->linesize[0] = -p->linesize[0];
        }
        bytestream2_init(&gb, buf, dsize);
        ff_msrle_decode(avctx, p, depth, &gb);
        if (height < 0) {
            p->data[0]    +=  p->linesize[0] * (avctx->height - 1);
            p->linesize[0] = -p->linesize[0];
        }
	} else */
    /* {
        switch (bit_count) {
        
        case 24:
       
            for (i = 0; i < avctx->height; i++) {
                memcpy(ptr, buf, n);
                buf += n;
                ptr += linesize;
            }
            break;
        }
    }
    */
    if(bit_count == 24)
      {
	for (i = 0; i < avctx->height; i++) {
	  memcpy(ptr, buf, n);
	  buf += n;
	  ptr += linesize;
	}
      }
    else
      {
	av_log(avctx, AV_LOG_ERROR, "SPFF decoder is broken\n");
	return AVERROR_INVALIDDATA;
      }

    *got_frame = 1;

    return buf_size;
}

AVCodec ff_spff_decoder = {
    .name           = "spff",
    .long_name      = NULL_IF_CONFIG_SMALL("SPFF image (a project for CS 3505)"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_SPFF,
    .decode         = spff_decode_frame,
    .capabilities   = AV_CODEC_CAP_DR1,
};
