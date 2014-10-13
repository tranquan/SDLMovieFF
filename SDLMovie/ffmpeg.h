//
//  ffmpeg.h
//  SDLMovie
//
//  Created by Kenji on 10/13/14.
//  Copyright (c) 2014 Kenji. All rights reserved.
//

#ifndef __SDLMovie__ffmpeg__
#define __SDLMovie__ffmpeg__

#include <stdio.h>
#include <pthread.h>
#import <AudioToolbox/AudioToolbox.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avio.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avstring.h>

#ifdef __RESAMPLER__
#include <libavutil/opt.h>

#ifdef __LIBAVRESAMPLE__
#include <libavresample/avresample.h>
#endif

#ifdef __LIBSWRESAMPLE__
#include <libswresample/swresample.h>
#endif
#endif

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>
#include <math.h>

#define SDL_AUDIO_BUFFER_SIZE 1024 // bytes
#define MAX_AUDIO_FRAME_SIZE 192000 // bytes
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024) // 81920 bytes

struct PacketQueue {
  AVPacketList *first_pkt, *last_pkt;
  int nb_packets;
  int size;
  pthread_mutex_t mutex;
};
typedef struct PacketQueue PacketQueue;

struct VideoState {
  AudioStreamBasicDescription ca_asbd;
  unsigned int                ca_audio_buffer_size;
  
  AVFormatContext *pFormatCtx;
  int             videoStream, audioStream;

  int             av_sync_type;
  double          external_clock;
  int64_t         external_clock_time;
  int             seek_req;
  int             seek_flags;
  int64_t         seek_pos;

  double          audio_clock;
  AVStream        *audio_st;
  PacketQueue     audioq;
  AVFrame         audio_frame;
  uint8_t         audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
  unsigned int    audio_buf_size;
  unsigned int    audio_buf_index;
  AVPacket        audio_pkt;
  uint8_t         *audio_pkt_data;
  int             audio_pkt_size;
  int             audio_hw_buf_size;
  double          audio_diff_cum;
  double          audio_diff_avg_coef;
  double          audio_diff_threshold;
  int             audio_diff_avg_count;
  double          frame_timer;
  double          frame_last_pts;
  double          frame_last_delay;
  double          video_clock;
  double          video_current_pts;
  int64_t         video_current_pts_time;
  AVStream        *video_st;
  PacketQueue     videoq;
//  VideoPicture    pictq[VIDEO_PICTURE_QUEUE_SIZE];
  int             pictq_size, pictq_rindex, pictq_windex;
//  SDL_mutex       *pictq_mutex;
//  SDL_cond        *pictq_cond;
//  SDL_Thread      *parse_tid;
//  SDL_Thread      *video_tid;

  char            filename[1024];
  int             quit;

  AVIOContext     *io_context;
  struct SwsContext *sws_ctx;
};
typedef struct VideoState VideoState;

void check_error(OSStatus error, const char *operation);

void packet_queue_init(PacketQueue *q);
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
int stream_component_open(VideoState *is, int stream_index);

int audio_decode_frame(VideoState *is, double *pts_ptr);
void audioqueue_output_callback(void *inUserData,
                                AudioQueueRef inAQ,
                                AudioQueueBufferRef inCompleteAQBuffer);

#endif /* defined(__SDLMovie__ffmpeg__) */
