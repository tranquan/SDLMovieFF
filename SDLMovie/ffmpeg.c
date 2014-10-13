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
  
  offset = 0;
  while (len > 0) {
    if (is->audio_buf_index >= is->audio_buf_size) {
      audio_size = audio_decode_frame(is, &pts);
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