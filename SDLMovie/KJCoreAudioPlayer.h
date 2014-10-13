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

@end
