/*
 * CS 3505 Spring 2017
 * SPFF image format encoder
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

#include "libavutil/imgutils.h"
#include "libavutil/avassert.h"
#include "bytestream.h"
#include "avcodec.h"
#include "internal.h"

// Get AVCodecContext and make sure it is the correct color scheme
static av_cold int spff_encode_init(AVCodecContext *avctx){
  if(avctx->pix_fmt == AV_PIX_FMT_RGB8||avctx->pix_fmt == AV_PIX_FMT_BGR8)
    {
      avctx->bits_per_coded_sample = 8;
    }
  else {
    av_log(avctx, AV_LOG_INFO, "unsupported pixel format, only support RBG8\n");
    return AVERROR(EINVAL);
  }
  return 0;
}

// encode one frame, spff only support one frame only
static int spff_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
			     const AVFrame *pict, int *got_packet)
{
  const AVFrame * const p = pict;
  int n_bytes_image, n_bytes_per_row, n_bytes, i, hsize, ret;
  int pad_bytes_per_row, pal_entries = 0;
  const uint32_t *pal = NULL;
   uint32_t palette256[256];
  int bit_count = avctx->bits_per_coded_sample;
  uint8_t *ptr, *buf;

#if FF_API_CODED_FRAME
  FF_DISABLE_DEPRECATION_WARNINGS
    avctx->coded_frame->pict_type = AV_PICTURE_TYPE_I;
  avctx->coded_frame->key_frame = 1;
  FF_ENABLE_DEPRECATION_WARNINGS
#endif
   av_assert1(bit_count == 8); // assert correct color scheme: RGB8
  //assign RGB values into palette256
   avpriv_set_systematic_pal2(palette256, avctx->pix_fmt);
   pal = palette256; // rereference pal
   
   if (pal && !pal_entries) pal_entries = 1 << bit_count;
   n_bytes_per_row = (int64_t)avctx->width;
   pad_bytes_per_row = (4 - n_bytes_per_row) & 3;
   //calculate image size
   n_bytes_image = avctx->height * (n_bytes_per_row + pad_bytes_per_row);

  // STRUCTURE.field refer to the MSVC documentation for SPFFFILEHEADER
  // and related pages.
#define SIZE_SPFFFILEHEADER 10
#define SIZE_SPFFINFOHEADER 16
   // calculate header size = fileheader size + infoheader size
  hsize = SIZE_SPFFFILEHEADER + SIZE_SPFFINFOHEADER + (pal_entries << 2);
  n_bytes = n_bytes_image + hsize; // calculate filesize=header size+image size
   //Check AVPacket size and/or allocate data.
  if ((ret = ff_alloc_packet2(avctx, pkt, n_bytes, 0)) < 0)
    return ret;
  buf = pkt->data;
  bytestream_put_byte(&buf, 'S');                   // SPFFFILEHEADER.bfType
  bytestream_put_byte(&buf, 'F');                   // do.
  bytestream_put_le32(&buf, n_bytes);               // SPFFFILEHEADER.bfSize
  bytestream_put_le32(&buf, hsize);                 // SPFFFILEHEADER.bfOffBits

  bytestream_put_le32(&buf, SIZE_SPFFINFOHEADER);   // SPFFINFOHEADER.biSize
  bytestream_put_le32(&buf, avctx->width);          // SPFFINFOHEADER.biWidth
  bytestream_put_le32(&buf, avctx->height);         // SPFFINFOHEADER.biHeight
  bytestream_put_le16(&buf, 1);                     // SPFFINFOHEADER.biPlanes
  // we choose to represent each pixel with 
  // 3 bytes for red,green,blue. bit_count should be 24
  bytestream_put_le16(&buf, bit_count);             // SPFFINFOHEADER.biBitCount
  
  // SPFF files are bottom-to-top so we start from the end...
  ptr = p->data[0] + (avctx->height - 1) * p->linesize[0];
  buf = pkt->data + hsize;
  for(i = 0; i < avctx->height; i++) {
    //bit_count is always 8
    {
      // copies count bytes from the object pointed to by src to the object 
      // pointed to by dest. memcpy(destination, source, count bytes)
      memcpy(buf, ptr, n_bytes_per_row);
    }
    // move pointers
    buf += n_bytes_per_row;
    memset(buf, 0, pad_bytes_per_row);// put pad_bytes_per_row 0's into buf
    buf += pad_bytes_per_row;
    ptr -= p->linesize[0]; // ... and go back
  }

  pkt->flags |= AV_PKT_FLAG_KEY; // bitwise OR for flag values
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
  .pix_fmts       = (const enum AVPixelFormat[]){AV_PIX_FMT_RGB8, AV_PIX_FMT_BGR8,AV_PIX_FMT_NONE},
};
