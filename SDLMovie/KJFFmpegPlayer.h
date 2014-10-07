//
//  KJFFmpegPlayer.h
//  SDLMovie
//
//  Created by Kenji on 10/3/14.
//  Copyright (c) 2014 Kenji. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface KJFFmpegPlayer : NSObject

+ (KJFFmpegPlayer *)sharedInstance;

- (void)initFFmpeg;
- (int)playVideoFile:(NSString *)filePath;
- (int)playVideoSyncToAudioWithFile:(NSString *)filePath;
- (int)playVideoAudioSyncedWithFile:(NSString *)filePath;
- (int)playVideoAudioCanSeekWithFile:(NSString *)filePath;

@end
