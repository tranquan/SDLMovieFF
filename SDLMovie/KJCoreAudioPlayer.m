//
//  KJCoreAudioPlayer.m
//  SDLMovie
//
//  Created by Kenji on 10/9/14.
//  Copyright (c) 2014 Kenji. All rights reserved.
//

#import "KJCoreAudioPlayer.h"
#include "ffmpeg.h"

@implementation KJCoreAudioPlayer

+ (KJCoreAudioPlayer *)sharedInstance {
  static KJCoreAudioPlayer *instance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[KJCoreAudioPlayer alloc] init];
  });
  return instance;
}

- (int)playAudioWithFile:(NSString *)filePath {
  
  if (filePath.length <= 0) {
    return -1;
  }
  
  VideoState *is = av_mallocz(sizeof(VideoState));
  
  av_register_all();
  
  av_strlcpy(is->filename, [filePath UTF8String], 1024);

  NSThread *decodeThread = [[NSThread alloc] initWithTarget:self selector:@selector(decodeThreadMain:) object:[NSData dataWithBytes:is length:sizeof(*is)]];
  [decodeThread setThreadPriority:0.5];
  [decodeThread start];
  
  return 0;
}

- (void)decodeThreadMain:(NSData *)videoState {
  
  VideoState *is = (VideoState *)[videoState bytes];
  
  // open file path & get stream info
  AVFormatContext *pFormatCtx = NULL;
  if (avformat_open_input(&pFormatCtx, is->filename, NULL, NULL) != 0) {
    return;
  }
  is->pFormatCtx = pFormatCtx;
  
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
    return;
  }
  av_dump_format(pFormatCtx, 0, is->filename, 0);
  
  // find audio stream & open
  int audio_index = -1;
  is->audioStream = -1;
  for (int i = 0; i < pFormatCtx->nb_streams; i++) {
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO && audio_index < 0) {
      audio_index = i;
    }
  }

  if (audio_index >= 0) {
    stream_component_open(is, audio_index);
  }
  
  if (is->audioStream < 0) {
    fprintf(stderr, "%s: could not open codecs\n", is->filename);
    return;
  }
  
  // core audio queue
  // create stream desc
  AudioStreamBasicDescription asbd;
  asbd.mFormatID = kAudioFormatLinearPCM;
  asbd.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  asbd.mSampleRate = 44100.0;
  asbd.mFramesPerPacket = 1;
  asbd.mBytesPerFrame = 4;
  asbd.mBytesPerPacket = 4;
  asbd.mChannelsPerFrame = 2;
  asbd.mBitsPerChannel = 16;
  is->ca_asbd = asbd;
  
  // alloc queue
  AudioQueueRef queue;
  check_error(AudioQueueNewOutput(&asbd, audioqueue_output_callback, is, NULL, NULL, 0, &queue),
             "AudioQueueNewOutput failed");

  // init queue with three empty buffers
  is->ca_audio_buffer_size = 36*1024;
  AudioQueueBufferRef buffers[3];
  for (int i=0; i < 3; i++) {
    check_error(AudioQueueAllocateBuffer(queue, is->ca_audio_buffer_size, &buffers[i]),
               "AudioQueueAllocBuffer failed");
    memset(buffers[i]->mAudioData, 0, is->ca_audio_buffer_size);
    buffers[i]->mAudioDataByteSize = is->ca_audio_buffer_size;
    check_error(AudioQueueEnqueueBuffer(queue, buffers[i], 0, NULL),
               "AudioQueueEnqueueBuffer failed");
  }
  
  // start queue
  check_error(AudioQueueStart(queue, NULL), "AudioQueueStart failed");
  
  // init resamle if needed
#ifdef __RESAMPLER__
  
  if (audio_index >= 0 &&
      pFormatCtx->streams[audio_index]->codec->sample_fmt != AV_SAMPLE_FMT_S16) {
    
    is->audio_need_resample = 1;
    is->pResampledOut = NULL;
    is->pSwrCtx = NULL;
    
    printf("Configure resampler: ");
    
#ifdef __LIBAVRESAMPLE__
    printf("libAvResample\n");
    is->pSwrCtx = avresample_alloc_context();
#endif
    
#ifdef __LIBSWRESAMPLE__
    printf("libSwResample\n");
    is->pSwrCtx = swr_alloc();
#endif
    
    // Some MP3/WAV don't tell this so make assumtion that
    // They are stereo not 5.1
    if (pFormatCtx->streams[audio_index]->codec->channel_layout == 0 &&
        pFormatCtx->streams[audio_index]->codec->channels == 2) {
      pFormatCtx->streams[audio_index]->codec->channel_layout = AV_CH_LAYOUT_STEREO;
      
    } else if (pFormatCtx->streams[audio_index]->codec->channel_layout == 0 &&
               pFormatCtx->streams[audio_index]->codec->channels == 1) {
      pFormatCtx->streams[audio_index]->codec->channel_layout = AV_CH_LAYOUT_MONO;
      
    } else if (pFormatCtx->streams[audio_index]->codec->channel_layout == 0 &&
               pFormatCtx->streams[audio_index]->codec->channels == 0) {
      pFormatCtx->streams[audio_index]->codec->channel_layout = AV_CH_LAYOUT_STEREO;
      pFormatCtx->streams[audio_index]->codec->channels = 2;
    }
    
    av_opt_set_int(is->pSwrCtx, "in_channel_layout",
                   pFormatCtx->streams[audio_index]->codec->channel_layout, 0);
    av_opt_set_int(is->pSwrCtx, "in_sample_fmt",
                   pFormatCtx->streams[audio_index]->codec->sample_fmt, 0);
    av_opt_set_int(is->pSwrCtx, "in_sample_rate",
                   pFormatCtx->streams[audio_index]->codec->sample_rate, 0);
    
    av_opt_set_int(is->pSwrCtx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(is->pSwrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int(is->pSwrCtx, "out_sample_rate", 44100, 0);
    
#ifdef __LIBAVRESAMPLE__
    
    if (avresample_open(is->pSwrCtx) < 0) {
#else
      
      if (swr_init(is->pSwrCtx) < 0) {
#endif
        fprintf(stderr, " ERROR!! From Samplert: %d Hz Sample format: %s\n",
                pFormatCtx->streams[audio_index]->codec->sample_rate,
                av_get_sample_fmt_name(pFormatCtx->streams[audio_index]->codec->sample_fmt));
        fprintf(stderr, "         To 44100 Sample format: s16\n");
        is->audio_need_resample = 0;
        is->pSwrCtx = NULL;;
      }
    }
    
#endif
  
  // main decode loop
  AVPacket pkt, *packet = &pkt;
  for(;;) {
    
    if (is->quit) {
      break;
    }
    
    if (is->audioq.size > MAX_AUDIOQ_SIZE) {
      usleep(10);
      continue;
    }
    
    if (av_read_frame(is->pFormatCtx, packet) < 0) {
      if (is->pFormatCtx->pb->error == 0) {
        usleep(100);
        continue;
      }
      else {
        break;
      }
    }
    
    if (packet->stream_index == is->audioStream) {
      packet_queue_put(&is->audioq, packet);
    } else {
      av_free_packet(packet);
    }
  }
  
  while (!is->quit) {
    usleep(100);
  }
}

@end
