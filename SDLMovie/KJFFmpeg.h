//
//  KJFFmpeg.h
//  KJMovie
//
//  Created by Kenji on 9/18/14.
//  Copyright (c) 2014 Beyonz. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface KJFFmpeg : NSObject

+ (KJFFmpeg *)sharedInstance;

- (void)initFFmpeg;
- (int)readVideoInfoAndExtractSampleFramesWithFile:(NSString *)filePath;
- (int)readVideoInfoAndExtractYUVFramesWithFile:(NSString *)filePath;
- (int)playVideoOnlyBySDLWithFile:(NSString *)filePath;
- (int)playAudioOnlyBySDLWithFile:(NSString *)filePath;

@end
