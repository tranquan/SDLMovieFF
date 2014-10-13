//
//  ffmpeg.c
//  SDLMovie
//
//  Created by Kenji on 10/13/14.
//  Copyright (c) 2014 Kenji. All rights reserved.
//

#include "ffmpeg.h"

#pragma mark - utils

void check_error(OSStatus error, const char *operation) {
  
  if (error == noErr) return;
  
  char errorString[20];
  *(UInt32 *)(errorString + 1) = CFSwapInt32HostToBig(error);
  if (isprint(errorString[1]) && isprint(errorString[2]) &&
      isprint(errorString[3]) && isprint(errorString[4])) {
    errorString[0] = errorString[5] = '\'';
    errorString[6] = '\0';
  } else {
    sprintf(errorString, "%d", (int)error);
  }
  
  fprintf(stderr, "Error: %s (%s)\n", operation, errorString);
  exit(1);
}

void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  pthread_mutex_init(&q->mutex, NULL);
};

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {

  AVPacketList *pkt1;
//  if(pkt != &flush_pkt && av_dup_packet(pkt) < 0) {
//    return -1;
//  }
  if(av_dup_packet(pkt) < 0) {
    return -1;
  }
  pkt1 = av_malloc(sizeof(AVPacketList));
  if (!pkt1)
    return -1;
  pkt1->pkt = *pkt;
  pkt1->next = NULL;

  pthread_mutex_lock(&q->mutex);

  if (!q->last_pkt)
    q->first_pkt = pkt1;
  else
    q->last_pkt->next = pkt1;
  q->last_pkt = pkt1;
  q->nb_packets++;
  q->size += pkt1->pkt.size;

  pthread_mutex_unlock(&q->mutex);
  return 0;
}

int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
  
  AVPacketList *pkt1;
  int ret;

  pthread_mutex_lock(&q->mutex);

  for(;;) {

//    if(is->quit) {
//      ret = -1;
//      break;
//    }

    pkt1 = q->first_pkt;
    if (pkt1) {
      q->first_pkt = pkt1->next;
      if (!q->first_pkt)
        q->last_pkt = NULL;
      q->nb_packets--;
      q->size -= pkt1->pkt.size;
      *pkt = pkt1->pkt;
      av_free(pkt1);
      ret = 1;
      break;
    } else if (!block) {
      ret = 0;
      break;
    } else {
      ret = 0;
      break;
    }
  }
  
  pthread_mutex_unlock(&q->mutex);
  return ret;
}

int stream_component_open(VideoState *as, int stream_index) {
  
  AVFormatContext *pFormatCtx = as->pFormatCtx;
  AVCodecContext *codecCtx = NULL;
  AVCodec *codec = NULL;
  AVDictionary *optionsDict = NULL;
  
  if (stream_index < 0 || stream_index >= pFormatCtx->nb_streams) {
    return -1;
  }
  
  codecCtx = pFormatCtx->streams[stream_index]->codec;
  codec = avcodec_find_decoder(codecCtx->codec_id);
  
  if (!codec || (avcodec_open2(codecCtx, codec, &optionsDict) < 0)) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;
  }
  
  switch (codecCtx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
      as->audioStream = stream_index;
      as->audio_st = pFormatCtx->streams[stream_index];
      as->audio_buf_size = 0;
      as->audio_buf_index = 0;
      memset(&as->audio_pkt, 0, sizeof(as->audio_pkt));
      packet_queue_init(&as->audioq);
      break;
      
    default:
      break;
  }
  
  return 0;
}

#pragma mark - audio

void audioqueue_output_callback(void *inUserData,
                                AudioQueueRef inAQ,
                                AudioQueueBufferRef inCompleteAQBuffer)
{
  VideoState *is = (VideoState *)inUserData;
  long len = is->ca_audio_buffer_size;
  long len_write, offset, audio_size;
  double pts;

  // write rand() samples into buffer
//  memset(inCompleteAQBuffer->mAudioData, rand(), is->ca_audio_buffer_size);
//  inCompleteAQBuffer->mAudioDataByteSize = is->ca_audio_buffer_size;
//  check_error(AudioQueueEnqueueBuffer(inAQ, inCompleteAQBuffer, 0, NULL),
//             "AudioQueueEnqueueBuffer failed");
  
  memset(inCompleteAQBuffer->mAudioData, 0, is->ca_audio_buffer_size);
  offset = 0;
  while (len > 0) {
    if (is->audio_buf_index >= is->audio_buf_size) {
      audio_size = audio_decode_frame2(is, &pts);
      if (audio_size < 0) {
        is->audio_buf_size = is->ca_audio_buffer_size;
        memset(inCompleteAQBuffer->mAudioData, 0, is->ca_audio_buffer_size);
        inCompleteAQBuffer->mAudioDataByteSize = is->ca_audio_buffer_size;
      }
      else {
        is->audio_buf_size = (unsigned int)audio_size;
      }
      is->audio_buf_index = 0;
    }
    
    len_write = is->audio_buf_size - is->audio_buf_index;
    if (len_write > len) {
      len_write = len;
    }
        
    memcpy((inCompleteAQBuffer->mAudioData + offset), (uint8_t *)is->audio_buf, len_write);
    len -= len_write;
    offset += len_write;
    is->audio_buf_index += len_write;
  }
  
  inCompleteAQBuffer->mAudioDataByteSize = is->ca_audio_buffer_size;
  check_error(AudioQueueEnqueueBuffer(inAQ, inCompleteAQBuffer, 0, NULL),
              "AudioQueueEnqueueBuffer failed");
}

int audio_decode_frame(VideoState *is, double *pts_ptr) {

  int len1, data_size = 0;
  AVPacket *pkt = &is->audio_pkt;

  for(;;) {

    while(is->audio_pkt_size > 0) {

      int got_frame = 0;
      len1 = avcodec_decode_audio4(is->audio_st->codec, &is->audio_frame, &got_frame, pkt);

      if(len1 < 0) {
        // if error, skip this frame
        is->audio_pkt_size = 0;
        break;
      }

      if (got_frame) {
        data_size =
        av_samples_get_buffer_size
        (
         NULL,
         is->audio_st->codec->channels,
         is->audio_frame.nb_samples,
         is->audio_st->codec->sample_fmt,
         1
         );
        memcpy(is->audio_buf, is->audio_frame.data[0], data_size);
      }

      is->audio_pkt_data += len1;
      is->audio_pkt_size -= len1;

      if(data_size <= 0) {
        // No data yet, get more frames
        continue;
      }

      // We have data, return it and come back for more later
      return data_size;
    }

    if(pkt->data) {
      av_free_packet(pkt);
    }

    if(is->quit) {
      return -1;
    }

    // read next packet
    if(packet_queue_get(&is->audioq, pkt, 1) < 0) {
      return -1;
    }
    is->audio_pkt_data = pkt->data;
    is->audio_pkt_size = pkt->size;
  }

  return 0;
}

int audio_decode_frame2(VideoState *is, double *pts_ptr) {
  
  long len1, data_size = 0;
  AVPacket *pkt = &is->audio_pkt;
  double pts;
  int n = 0;
  
#ifdef __RESAMPLER__
  long resample_size = 0;
#endif
  
  for(;;) {
    while(is->audio_pkt_size > 0) {
      int got_frame = 0;
      len1 = avcodec_decode_audio4(is->audio_st->codec, &is->audio_frame, &got_frame, pkt);
      fprintf(stderr, "frame rate: %d", is->audio_frame.nb_samples);
      
      if(len1 < 0) {
        /* if error, skip frame */
        is->audio_pkt_size = 0;
        break;
      }
      
      if(got_frame) {
        data_size =
        av_samples_get_buffer_size
        (
         NULL,
         is->audio_st->codec->channels,
         is->audio_frame.nb_samples,
         is->audio_st->codec->sample_fmt,
         1
         );
        
#ifdef __RESAMPLER__
        
        if (is->audio_need_resample == 1) {
          resample_size = audio_tutorial_resample(is, &is->audio_frame);
          if (resample_size > 0 ) {
            memcpy(is->audio_buf, is->pResampledOut, resample_size);
            memset(is->pResampledOut, 0x00, resample_size);
          }
          
        } else {
          
#endif
          memcpy(is->audio_buf, is->audio_frame.data[0], data_size);
        
#ifdef __RESAMPLER__
        }
#endif
      }
      
      is->audio_pkt_data += len1;
      is->audio_pkt_size -= len1;
      
      if(data_size <= 0) {
        /* No data yet, get more frames */
        continue;
      }
      
      pts = is->audio_clock;
      *pts_ptr = pts;
      n = 2 * is->audio_st->codec->channels;
      
#ifdef __RESAMPLER__
      
      // If you just return original data_size you will suffer
      // for clicks because you don't have that much data in
      // queue incoming so return resampled size.
      if(is->audio_need_resample == 1) {
        is->audio_clock += (double)resample_size /
        (double)(n * is->audio_st->codec->sample_rate);
        return (int)resample_size;
        
      } else {
#endif
        // We have data, return it and come back for more later
        is->audio_clock += (double)data_size /
        (double)(n * is->audio_st->codec->sample_rate);
        return (int)data_size;
#ifdef __RESAMPLER__
      }
      
#endif
    }
    
    if(pkt->data) {
      av_free_packet(pkt);
    }
    
    if(is->quit) {
      return -1;
    }
    
    /* next packet */
    if(packet_queue_get(&is->audioq, pkt, 1) < 0) {
      return -1;
    }
    
//    if(pkt->data == flush_pkt.data) {
//      avcodec_flush_buffers(is->audio_st->codec);
//      continue;
//    }
    
    is->audio_pkt_data = pkt->data;
    is->audio_pkt_size = pkt->size;
    
    /* if update, update the audio clock w/pts */
    if(pkt->pts != AV_NOPTS_VALUE) {
      is->audio_clock = av_q2d(is->audio_st->time_base) * pkt->pts;
    }
  }
}

long audio_tutorial_resample(VideoState *is, struct AVFrame *inframe) {
  
#ifdef __RESAMPLER__
  
#ifdef __LIBAVRESAMPLE__
  
  // There is pre 1.0 libavresample and then there is above..
#if LIBAVRESAMPLE_VERSION_MAJOR == 0
  void **resample_input_bytes = (void **)inframe->extended_data;
#else
  uint8_t **resample_input_bytes = (uint8_t **)inframe->extended_data;
#endif
  
#else
  uint8_t **resample_input_bytes = (uint8_t **)inframe->extended_data;
#endif
  
  int resample_nblen = 0;
  long resample_long_bytes = 0;
  
  if( is->pResampledOut == NULL || inframe->nb_samples > is->resample_size) {
    
#if __LIBAVRESAMPLE__
    is->resample_size = av_rescale_rnd(avresample_get_delay(is->pSwrCtx) +
                                       inframe->nb_samples,
                                       44100,
                                       44100,
                                       AV_ROUND_UP);
#else
    is->resample_size = av_rescale_rnd(swr_get_delay(is->pSwrCtx, 44100) +
                                       inframe->nb_samples,
                                       44100,
                                       44100,
                                       AV_ROUND_UP);
#endif
    
    if(is->pResampledOut != NULL) {
      av_free(is->pResampledOut);
      is->pResampledOut = NULL;
    }
    
    av_samples_alloc(&is->pResampledOut, &is->resample_lines, 2, is->resample_size,
                     AV_SAMPLE_FMT_S16, 0);
    
  }
  
  
#ifdef __LIBAVRESAMPLE__
  
  // OLD API (0.0.3) ... still NEW API (1.0.0 and above).. very frustrating..
  // USED IN FFMPEG 1.0 (LibAV SOMETHING!). New in FFMPEG 1.1 and libav 9
#if LIBAVRESAMPLE_VERSION_INT <= 3
  // AVResample OLD
  resample_nblen = avresample_convert(is->pSwrCtx, (void **)&is->pResampledOut, 0,
                                      is->resample_size,
                                      (void **)resample_input_bytes, 0, inframe->nb_samples);
#else
  //AVResample NEW
  resample_nblen = avresample_convert(is->pSwrCtx, (uint8_t **)&is->pResampledOut,
                                      0, is->resample_size,
                                      (uint8_t **)resample_input_bytes, 0, inframe->nb_samples);
#endif
  
#else
  // SWResample
  resample_nblen = swr_convert(is->pSwrCtx, (uint8_t **)&is->pResampledOut,
                               is->resample_size,
                               (const uint8_t **)resample_input_bytes, inframe->nb_samples);
#endif
  
  resample_long_bytes = av_samples_get_buffer_size(NULL, 2, resample_nblen,
                                                   AV_SAMPLE_FMT_S16, 1);
  
  if (resample_nblen < 0) {
    fprintf(stderr, "reSample to another sample format failed!\n");
    return -1;
  }
  
  return resample_long_bytes;
  
#else
  return -1;
#endif
}