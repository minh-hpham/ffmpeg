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
#include "spff.h"
#include "internal.h"

static av_cold int spff_encode_init(AVCodecContext *avctx){
  if(avctx->pix_fmt == AV_PIX_FMT_BGR24)
  {
    // no need for color table
    // PROB: 3 bits per pixel (require is 1 bit)
    // can do compression if desire.
    avctx->bits_per_coded_sample = 24;
  }
  else {
    av_log(avctx, AV_LOG_INFO, "unsupported pixel format\n");
    return AVERROR(EINVAL);
  }
  return 0;
}

static int spff_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
			     const AVFrame *pict, int *got_packet)
{
  const AVFrame * const p = pict;
  int n_bytes_image, n_bytes_per_row, n_bytes, i, hsize, ret;
  const uint32_t *pal = NULL;
  int pad_bytes_per_row, pal_entries = 0, compression = SPFF_RGB;
  int bit_count = avctx->bits_per_coded_sample;
  uint8_t *ptr, *buf;

#if FF_API_CODED_FRAME
  FF_DISABLE_DEPRECATION_WARNINGS
    avctx->coded_frame->pict_type = AV_PICTURE_TYPE_I;
  avctx->coded_frame->key_frame = 1;
  FF_ENABLE_DEPRECATION_WARNINGS
#endif
  if (pal && !pal_entries) pal_entries = 1 << bit_count;
   n_bytes_per_row = ((int64_t)avctx->width * (int64_t)3);
   pad_bytes_per_row = (4 - n_bytes_per_row) & 3;
   n_bytes_image = avctx->height * (n_bytes_per_row + pad_bytes_per_row);
  // STRUCTURE.field refer to the MSVC documentation for BITMAPFILEHEADER
  // and related pages.
#define SIZE_SPFFFILEHEADER 10
#define SIZE_SPFFINFOHEADER 24
  hsize = SIZE_SPFFFILEHEADER + SIZE_SPFFINFOHEADER + (pal_entries << 2);
  n_bytes = n_bytes_image + hsize;
   //Check AVPacket size and/or allocate data.
  if ((ret = ff_alloc_packet2(avctx, pkt, n_bytes, 0)) < 0)
    return ret;
  buf = pkt->data;
  bytestream_put_byte(&buf, 'S');                   // BITMAPFILEHEADER.bfType
  bytestream_put_byte(&buf, 'F');                   // do.
  bytestream_put_le32(&buf, n_bytes);               // BITMAPFILEHEADER.bfSize
  bytestream_put_le32(&buf, hsize);                 // BITMAPFILEHEADER.bfOffBits
  bytestream_put_le32(&buf, SIZE_SPFFINFOHEADER);   // BITMAPINFOHEADER.biSize
  bytestream_put_le32(&buf, avctx->width);          // BITMAPINFOHEADER.biWidth
  bytestream_put_le32(&buf, avctx->height);         // BITMAPINFOHEADER.biHeight
  bytestream_put_le16(&buf, 1);                     // BITMAPINFOHEADER.biPlanes
  // we choose to represent each pixel with 
  // 3 bytes for red,green,blue. bit_count should be 24
  bytestream_put_le16(&buf, bit_count);             // BITMAPINFOHEADER.biBitCount
  // choose not to compress-> compression=0
  bytestream_put_le32(&buf, compression);           // BITMAPINFOHEADER.biCompression
  // maybe 0 for uncompressed images
  bytestream_put_le32(&buf, 0); 
  //bytestream_put_le32(&buf, n_bytes_image);         // BITMAPINFOHEADER.biSizeImage
  for (i = 0; i < pal_entries; i++)
    bytestream_put_le32(&buf, pal[i] & 0xFFFFFF);
  // BMP files are bottom-to-top so we start from the end...
  ptr = p->data[0] + (avctx->height - 1) * p->linesize[0];
  buf = pkt->data + hsize;
  for(i = 0; i < avctx->height; i++) {
    //bit_count is always 24
    {
      memcpy(buf, ptr, n_bytes_per_row);
    }
    buf += n_bytes_per_row;
    memset(buf, 0, pad_bytes_per_row);
    buf += pad_bytes_per_row;
    ptr -= p->linesize[0]; // ... and go back
  }

  pkt->flags |= AV_PKT_FLAG_KEY;
  *got_packet = 1;
  return 0;
}

AVCodec ff_spff_encoder = {
  .name           = "spff",
  .long_name      = NULL_IF_CONFIG_SMALL("SPFF image (a project for CS 3505)"),
  .type           = AVMEDIA_TYPE_VIDEO,
  .id             = AV_CODEC_ID_SPFF,
  .init           = spff_encode_init,
  .encode2        = spff_encode_frame,
  .pix_fmts       = (const enum AVPixelFormat[]){AV_PIX_FMT_BGR24,AV_PIX_FMT_NONE },
};
