#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

int main(int argc, char *argv[]){
  // confirm there is filename passed in
  if(argc < 2)
    return -1;
  // check file extension
  const char * ext = strrchr(argv[1], '.');
  if((!ext) || (strcmp(ext, ".jpg")!=0)) {
    return -1;
  }
  
  AVFormatContext *pFormatCtx = NULL;
  int             videoStream=0,i,ret;
  AVCodecContext  *pCodecCtx = NULL;
  AVCodecContext  *pCodecCtxOrig = NULL;
  AVCodec         *pCodec = NULL;
  AVFrame         *pFrame = NULL; 
  AVFrame         *pFrameRGB = NULL;
  AVPacket        *packet=av_packet_alloc();
  int             frameFinished;
  int             numBytes;
  uint8_t         *buffer = NULL;
  char *filename = argv[1];
  //register all codecs
  av_register_all();
  //open file
  ret = avformat_open_input(&pFormatCtx, argv[1], NULL, NULL);

  if (ret<0) abort();
  
  // Retrieve stream information
  if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    return -1; // Couldn't find stream information

  // Dump information about file onto standard error
  av_dump_format(pFormatCtx, 0, argv[1], 0);

  // Find the first video stream
  videoStream=-1;
  for(i=0; i<pFormatCtx->nb_streams; i++)
    if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {
      videoStream=i;
      break;
    }
  av_log(pFormatCtx,AV_LOG_INFO,"PRINT AFTER FIND VIDEO STREAM, VIDEOSTREAM = %d \n", videoStream);
  if(videoStream==-1)
    return -1; // Didn't find a video stream

 
  // Find the decoder for jpg
  pCodec = avcodec_find_decoder(pFormatCtx->streams[i]->codecpar->codec_id);
 
  if (pCodec==NULL)
    {
      return -1; // cannot find codec
    }

  //copy codec context before opening codec
  pCodecCtx=avcodec_alloc_context3(pCodec);

  // Get a pointer to the codec context for the video stream
  avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);

 
  // Open codec
  if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
    {
      return -1; // cannot find codec
    }
  av_log(pFormatCtx, AV_LOG_INFO, "PRINT CODEC HIGHT %d, Width %d \n", pCodecCtx->height, pCodecCtx->width);

  
 
  switch (pCodecCtx->pix_fmt) {
  case AV_PIX_FMT_YUVJ420P :
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    break;
  case AV_PIX_FMT_YUVJ422P  :
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV422P;
    break;
  case AV_PIX_FMT_YUVJ444P   :
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV444P;
    break;
  case AV_PIX_FMT_YUVJ440P :
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV440P;
  default:
    pCodecCtx->pix_fmt = pCodec->pix_fmts[0];
    break;
  }


  av_log(pCodecCtx,AV_LOG_INFO,"PRINT BEFORE GET CACHED CONTEXT format %d \n",av_get_pix_fmt_name(pCodecCtx->pix_fmt));

  av_log(pCodecCtx,AV_LOG_INFO,"PRINT BEFORE GET CACHED CONTEXT, WIDTH %d, HEIGHT %d, format %d \n",pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt);
  
  // use swscaling to convert jpeg to RGB24
  struct SwsContext *sws_ctx = NULL;

  // scaling to open new image under RGB24
  sws_ctx = sws_getCachedContext(NULL,
				 pCodecCtx->width,
				 pCodecCtx->height,
				 pCodecCtx->pix_fmt,
				 pCodecCtx->width,
				 pCodecCtx->height,
				 AV_PIX_FMT_RGB24,
				 SWS_BICUBIC, 
				 NULL, NULL,NULL);
  av_log(pFormatCtx,AV_LOG_INFO,"PRINT AFTER GET CACHED CONTEXT, WIDTH %d, HEIGHT %d, format %d \n",pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt);

// allocate frame
  pFrame = av_frame_alloc();
  // Allocate an AVFrame structure
  pFrameRGB=av_frame_alloc();
  if(pFrameRGB==NULL)
    return -1;
  // Determine required buffer size and allocate buffer
  numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width,pCodecCtx->height, 32);
  buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
  av_log(pFormatCtx,AV_LOG_INFO,"PRINT BEFORE AV IMAGE FILL, numBytes %d\n", numBytes);
  // Assign appropriate parts of buffer to image planes in pFrameRGB
  av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 32);

  if (av_read_frame(pFormatCtx, packet)>=0){
    // Is this a packet from the video stream?
    if(packet->stream_index==videoStream) {
     
      // Decode video frame
      int ret = avcodec_send_packet(pCodecCtx, packet);
      ret = avcodec_receive_frame(pCodecCtx, pFrame);
     
    
      av_log(pFormatCtx,AV_LOG_INFO,"PRINT BEFORE SWS SCALE\n");
      
      // Did we get a video frame?
      if(ret) {
	// Convert the image from its native format to RGB
	sws_scale(sws_ctx, 
		  (uint8_t const * const *)(pFrame->data),
		  pFrame->linesize,
		  0,
		  pCodecCtx->height,
		  pFrameRGB->data, 
		  pFrameRGB->linesize);
	av_log(pFormatCtx,AV_LOG_INFO,"PRINT AFTER SWS SCALE\n");
	av_packet_unref(packet);
      }
    }
  }
  // create spff files
  FILE *file;
  char spfffilename[32];
  AVCodec *spff_codec;
  AVCodecContext *spff_context=NULL;
  AVPacket spff_packet;
  
  // Find the codec for SPFF and allocate the context

  spff_codec= avcodec_find_encoder(AV_CODEC_ID_SPFF);
  av_log(pFormatCtx,AV_LOG_INFO,"PRINT AFTER FIND ENCODER\n");
  spff_context = avcodec_alloc_context3(spff_codec);
  av_log(pFormatCtx,AV_LOG_INFO,"PRINT AFTER GOT ENCODER CONTEXT\n");

  // set context variables
  spff_context->width=pCodecCtx->width;
  spff_context->height=pCodecCtx->height;
  spff_context->pix_fmt = spff_codec->pix_fmts[0];
  spff_context->time_base=(AVRational){1,1};
  av_log(spff_context,AV_LOG_INFO,"PRINT AFTER SETTING VARIABLES FOR ENCODER CONTEXT: width %d height %d pix_fmt %d\n",spff_context->width,spff_context->height,spff_context->pix_fmt);
  av_log(spff_context,AV_LOG_INFO,"PRINT AFTER OPENED ENCODER %d\n",ret);

  // open codec
  ret = avcodec_open2(spff_context, spff_codec, NULL);

  sprintf(spfffilename,"frame%d.spff",100); //generate filename
  file = fopen(spfffilename,"wb"); //create a file using the filename
  
  // prepare the AVframe to be written to file
  
  pFrameRGB->format= spff_context->pix_fmt;
  pFrameRGB->width=pCodecCtx->width;
  pFrameRGB->height=pCodecCtx->height;
  
  // initialize spff packet to contain the frame
  av_init_packet(&spff_packet);
  spff_packet.data=NULL; // null for now, because it will be allocated later by the spff encoder
  spff_packet.size=0; //0 for now, will be changed by encoder
  av_log(spff_context,AV_LOG_INFO,"PRINT AFTER FINISHED WITH PACKET\n");

  // encode the spff_frame in spff format
  ret=avcodec_send_frame(spff_context, pFrameRGB);
  av_log(pFormatCtx,AV_LOG_INFO,"PRINT AFTER SEND FRAME");
  ret=avcodec_receive_packet(spff_context, &spff_packet);
  av_log(spff_context,AV_LOG_INFO,"PRINT AFTER FINISHED ENCODING\n");

  // write all bytes of spff_packet to file
  fwrite(spff_packet.data, 1, spff_packet.size, file);
  av_log(spff_context,AV_LOG_INFO,"PRINT AFTER WRITTING TO FILE\n");

  // free memory
  av_free (buffer);
  av_free (pFrame);
  av_free (pFrameRGB);
  av_packet_unref(&spff_packet);

  // Close the codecs
  avcodec_close(pCodecCtx);
  avcodec_close(pCodecCtxOrig);
  avcodec_close(spff_context);


  // Close the files
  avformat_close_input(&pFormatCtx);
  fclose(file);
  
  return 0;
}
