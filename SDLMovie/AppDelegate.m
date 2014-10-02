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
  
  NSString *filePath = @"/Users/Kenji/Documents/wow-lichking-360.mp4";
  
//  [[KJFFmpeg sharedInstance] readVideoInfoAndExtractSampleFramesWithFile:filePath];
  
//  [[KJFFmpeg sharedInstance] readVideoInfoAndExtractYUVFramesWithFile:filePath];
  
//  [[KJFFmpeg sharedInstance] playVideoOnlyBySDLWithFile:filePath];
  
  [[KJFFmpeg sharedInstance] playAudioOnlyBySDLWithFile:filePath];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
  // Insert code here to tear down your application
}

@end
