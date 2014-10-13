//
//  KJCoreAudioPlayer.h
//  SDLMovie
//
//  Created by Kenji on 10/9/14.
//  Copyright (c) 2014 Kenji. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface KJCoreAudioPlayer : NSObject

+ (KJCoreAudioPlayer *)sharedInstance;
- (int)playAudioWithFile:(NSString *)filePath;

// open and ready to play
// filePath can be stream or local file
- (int)openAudioWithFile:(NSString *)filePath;

// controls
- (int)play;
- (int)pause;
- (int)stop;

@end
