/*
 * Peter's codec encoder - high quality (RGB24), no compression.
 */

#include "libavutil/imgutils.h"
#include "libavutil/avassert.h"
#include "avcodec.h"
#include "bytestream.h"
#include "internal.h"

static av_cold int spff_encode_init(AVCodecContext *avctx)
{
  // Make sure we are receiving the correct type of image.
  
  if (avctx->pix_fmt != AV_PIX_FMT_RGB24)
  {
    av_log(avctx, AV_LOG_ERROR, "Only RGB24 is supported\n");
    return AVERROR(EINVAL);
  }
  
  // Set up the context for receiving a picture.
  
  avctx->bits_per_coded_sample = 24;  // Specify rgb24

  // Indicate success.
  
  return 0;
}


static int spff_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                            const AVFrame *pic, int *got_packet)
{
    int result, x, y, b;
    uint8_t *output_data;
    
    // Debugging variables.
    
    static int reported = 0;

    // Debugging message.
    
    if (!reported)
    {
      av_log(avctx, AV_LOG_INFO, "*** CS 3505:  Executing in spffenc.c\n");
      av_log(avctx, AV_LOG_INFO, "*** CS 3505:  Codec by Peter Jensen ***\n");
      reported = 1;
    }
    
    // Allocate an output memory buffer.

    
    result = ff_alloc_packet2(avctx, pkt,
			      12 + avctx->width * avctx->height * 3,
			      12 + avctx->width * avctx->height * 3);

    if (result < 0)
        return result;

    output_data = pkt->data;

    // Write the header.    

    bytestream_put_byte(&output_data, 's');
    bytestream_put_byte(&output_data, 'p');
    bytestream_put_byte(&output_data, 'f');
    bytestream_put_byte(&output_data, 'f');

    // Meaningless byte to block students who are incorrectly using this format.
    //   (Not relevant in 2017, but I'm leaving it and the other harmless
    //   flaw in.)
    bytestream_put_byte(&output_data, ((char) output_data));  
    
    bytestream_put_le32(&output_data, avctx->width);
    bytestream_put_be32(&output_data, avctx->height);

    // Write the bytes.
    
    for (y = 0; y < avctx->height; y++)
      for (x = 0; x < avctx->width; x++)
	for (b = 0; b < 3; b++)
          bytestream_put_byte(&output_data, pic->data[0][y*pic->linesize[0]+x*3+b]);

    // Cite: bmp - unknown why a single frame is marked as a key frame.

    pkt->flags |= AV_PKT_FLAG_KEY;

    *got_packet = 1;
    return 0;
}


static av_cold int spff_encode_close(AVCodecContext *avctx)
{
  // Indicate success.
  
  return 0;
}


AVCodec ff_spff_encoder = {
    .name           = "spff",
    .long_name      = NULL_IF_CONFIG_SMALL("Peter's spff encoder"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_SPFF,
    .init           = spff_encode_init,
    .encode2        = spff_encode_frame,
    .close          = spff_encode_close,
    .pix_fmts       = (const enum AVPixelFormat[]){
    AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE
    },
};
