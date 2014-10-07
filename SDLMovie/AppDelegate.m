//
//  AppDelegate.m
//  SDLMovie
//
//  Created by Kenji on 10/2/14.
//  Copyright (c) 2014 Kenji. All rights reserved.
//

#import "AppDelegate.h"
#import "KJFFmpeg.h"
#import "KJFFmpegPlayer.h"

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  
  NSString *filePath = @"/Users/Kenji/Documents/fmle.mp4";
//  NSString *filePath = @"rtp://192.168.0.51:5004/";
  
//  int ret = [[KJFFmpeg sharedInstance] readVideoInfoAndExtractSampleFramesWithFile:filePath];
  
//  [[KJFFmpeg sharedInstance] readVideoInfoAndExtractYUVFramesWithFile:filePath];
  
//  int ret = [[KJFFmpeg sharedInstance] playVideoOnlyBySDLWithFile:filePath];
  
  // tut 3: play video only
//  int ret = [[KJFFmpeg sharedInstance] playAudioOnlyBySDLWithFile:filePath];
  
  // tut 4: play video with audio
//  int ret = [[KJFFmpegPlayer sharedInstance] playVideoFile:filePath];
  
  // tut 5: play video sync to audio
//  int ret = [[KJFFmpegPlayer sharedInstance] playVideoSyncToAudioWithFile:filePath];
  
  // tut 6: play video, audio can sync to: audio (default), video or system clock
//  int ret = [[KJFFmpegPlayer sharedInstance] playVideoAudioSyncedWithFile:filePath];
  
  // tut 7: play video with seek feature
  int ret = [[KJFFmpegPlayer sharedInstance] playVideoAudioCanSeekWithFile:filePath];
  
  NSLog(@"result: %@", (ret == 0 ? @"SUCCESS" : @"FAILED"));
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
  // Insert code here to tear down your application
}

@end
