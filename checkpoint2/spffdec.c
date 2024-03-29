/*
 * CS 3505 Spring 2017
 * SPFF image format decoder
 * only handle image in RBG8 color
 * Code base on bmp format in ffmpeg
 * By Minh Pham and To Tang
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
   
    unsigned int ihsize;
    int i, n, linesize, ret;

    uint8_t *ptr;
    int dsize;
    const uint8_t *buf0 = buf;

    if (buf_size < 10) {
        av_log(avctx, AV_LOG_ERROR, "buf size too small (%d)\n", buf_size);
        return AVERROR_INVALIDDATA;
    }

    if (bytestream_get_byte(&buf) != 'S' ||
        bytestream_get_byte(&buf) != 'F') {
        av_log(avctx, AV_LOG_ERROR, "bad magic number\n");
        return AVERROR_INVALIDDATA;
    }

    fsize = bytestream_get_le32(&buf); // filesize = image size + header size
    if (buf_size < fsize) {
        av_log(avctx, AV_LOG_ERROR, "not enough data (%d < %u), trying to decode anyway\n",
               buf_size, fsize);
        fsize = buf_size;
    }
    if(avctx->width < 0 || avctx->height <0){
       av_log(avctx, AV_LOG_ERROR, "negative width or height\n");
    }
    
    hsize  = bytestream_get_le32(&buf); /* fileheader+infoheader size */
    ihsize = bytestream_get_le32(&buf); /* info header size */
    if (ihsize + 10LL > hsize) {
        av_log(avctx, AV_LOG_ERROR, "invalid header size %u\n", hsize);
        return AVERROR_INVALIDDATA;
    }

    /* sometimes file size is set to some headers size, set a real size in that case */
    if (fsize == 10 || fsize == ihsize + 10)
        fsize = buf_size - 2;

    if (fsize <= hsize) {
        av_log(avctx, AV_LOG_ERROR,
               "Declared file size is less than header size (%u < %u)\n",
               fsize, hsize);
        return AVERROR_INVALIDDATA;
    }

    width  = bytestream_get_le32(&buf); // get width
    height = bytestream_get_le32(&buf); // get height
   

    /* planes */
    if (bytestream_get_le16(&buf) != 1) { // make sure only 1 plane
        av_log(avctx, AV_LOG_ERROR, "invalid SPFF header\n");
        return AVERROR_INVALIDDATA;
    }

    bit_count = bytestream_get_le16(&buf); // bit_count always =8 (RBG8)

   
    avctx->width  = width;
    avctx->height = height > 0 ? height : -height; // make sure height is positive
    avctx->pix_fmt = AV_PIX_FMT_NONE; // set default pixel format

    if(bit_count == 8)
      avctx->pix_fmt = AV_PIX_FMT_BGR8; // set pixel format to BGR8
    else
      {
        av_log(avctx, AV_LOG_ERROR, "depth %u not supported\n", bit_count);
        return AVERROR_INVALIDDATA;
      }
    if (avctx->pix_fmt == AV_PIX_FMT_NONE) {
        av_log(avctx, AV_LOG_ERROR, "unsupported pixel format\n");
        return AVERROR_INVALIDDATA;
    }

     // ff_get_buffer(AVCodecContext, AVFrame, int), get buffer for a frame
    if ((ret = ff_get_buffer(avctx, p, 0)) < 0)
        return ret;

    p->pict_type = AV_PICTURE_TYPE_I; // frame's picture type = Intra
    p->key_frame = 1;

    buf   = buf0 + hsize;
    dsize = buf_size - hsize;

    /* Line size in file multiple of 4 */
    n = ((avctx->width * bit_count + 31) / 8) & ~3;
    if (n * avctx->height > dsize) {
        n = (avctx->width * bit_count + 7) / 8;
        if (n * avctx->height > dsize) {
            av_log(avctx, AV_LOG_ERROR, "not enough data (%d < %d)\n",
                   dsize, n * avctx->height);
            return AVERROR_INVALIDDATA;
        }
        av_log(avctx, AV_LOG_ERROR, "data size too small, assuming missing line alignment\n");
    }
    // set pointer
    ptr      = p->data[0] + (avctx->height - 1) * p->linesize[0];
    linesize = -p->linesize[0];

    if(bit_count == 8)
      {
	for (i = 0; i < avctx->height; i++) {
	  // copies count bytes from the object pointed to by src to the object 
	  // pointed to by dest. memcpy(destination, source, count bytes)
	  memcpy(ptr, buf, n); 
	  //move pointers
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
