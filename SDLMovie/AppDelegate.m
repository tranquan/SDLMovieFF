//
//  AppDelegate.m
//  SDLMovie
//
//  Created by Kenji on 10/2/14.
//  Copyright (c) 2014 Kenji. All rights reserved.
//

#import "AppDelegate.h"
#import "KJFFmpeg.h"

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  
  NSString *filePath = @"/Users/Kenji/Documents/starcraft-opening-mono.mp4";
  
//  [[KJFFmpeg sharedInstance] readVideoInfoAndExtractSampleFramesWithFile:filePath];
  
//  [[KJFFmpeg sharedInstance] readVideoInfoAndExtractYUVFramesWithFile:filePath];
  
//  [[KJFFmpeg sharedInstance] playVideoOnlyBySDLWithFile:filePath];
  
  int ret = [[KJFFmpeg sharedInstance] playAudioOnlyBySDLWithFile:filePath];
  NSLog(@"result: %@", (ret == 0 ? @"SUCCESS" : @"FAILED"));
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
  // Insert code here to tear down your application
}

@end
