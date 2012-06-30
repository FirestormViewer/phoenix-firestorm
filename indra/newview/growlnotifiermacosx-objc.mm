/* Copyright (c) 2010 Katharine Berry All rights reserved.
 *
 * @file growlnotifiermacosx-objc.mm
 * @Implementation of desktop notification system (aka growl).
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Katharine Berry nor the names of any contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KATHARINE BERRY AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KATHARINE BERRY OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "growlnotifiermacosx-objc.h"
#import <Cocoa/Cocoa.h>
#import "Growl/Growl.h"

// Make a happy delegate (that does nothing other than the required response to a registration dictionary)
@interface FirestormBridge : NSObject <GrowlApplicationBridgeDelegate>
@end

@implementation FirestormBridge

- (NSDictionary *) registrationDictionaryForGrowl {
	
	NSDictionary *regDict = [NSDictionary dictionaryWithObjectsAndKeys:
							 @"Firestorm Viewer", GROWL_APP_NAME,
							 nil];
		
	return regDict;
}

@end

bool isInstalled = false;

void growlApplicationBridgeNotify(const std::string& withTitle, const std::string& description, const std::string& notificationName, 
                                void *iconData, unsigned int iconDataSize, int priority, bool isSticky) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    Class GAB = NSClassFromString(@"GrowlApplicationBridge");
	if([GAB respondsToSelector:@selector(notifyWithTitle:description:notificationName:iconData:priority:isSticky:clickContext:identifier:)]) {
        [GAB notifyWithTitle:[NSString stringWithCString:withTitle.c_str() encoding:NSUTF8StringEncoding]
                                description:[NSString stringWithCString:description.c_str() encoding:NSUTF8StringEncoding]
                           notificationName:[NSString stringWithCString:notificationName.c_str() encoding:NSUTF8StringEncoding]
                                   iconData:(iconData == NULL ? nil : [NSData dataWithBytes:iconData length:iconDataSize])
                                   priority:priority
                                   isSticky:isSticky
                               clickContext:nil];
        //NSLog(@"Growl msg: %@:%@", [NSString stringWithCString:withTitle.c_str() encoding:NSUTF8StringEncoding],              [NSString stringWithCString:description.c_str() encoding:NSUTF8StringEncoding]);
    }
    [pool release];
}

void growlApplicationBridgeInit() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSBundle *mainBundle = [NSBundle mainBundle];
	NSString *path = [[mainBundle privateFrameworksPath] stringByAppendingPathComponent:@"Growl"];
	if(NSAppKitVersionNumber >= 1038)
		path = [path stringByAppendingPathComponent:@"1.3"];
	else
		path = [path stringByAppendingPathComponent:@"1.2.3"];
	
	path = [path stringByAppendingPathComponent:@"Growl.framework"];
	//NSLog(@"path: %@", path);
	NSBundle *growlFramework = [NSBundle bundleWithPath:path];
	if([growlFramework load])
	{
		NSDictionary *infoDictionary = [growlFramework infoDictionary];
		NSLog(@"Using Growl.framework %@ (%@)",
              [infoDictionary objectForKey:@"CFBundleShortVersionString"],
			  [infoDictionary objectForKey:(NSString *)kCFBundleVersionKey]);
        
        FirestormBridge *delegate = [[[FirestormBridge alloc] init] autorelease];

        Class GAB = NSClassFromString(@"GrowlApplicationBridge");
		if([GAB respondsToSelector:@selector(setGrowlDelegate:)])
			[GAB performSelector:@selector(setGrowlDelegate:) withObject:delegate];
        isInstalled = true;
    }
    
    [pool release];
}

bool growlApplicationBridgeIsGrowlInstalled() {
    return isInstalled;
}

void growlApplicationBridgeRegister(const std::string& appname, const std::set<std::string>& notifications)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    Class GAB = NSClassFromString(@"GrowlApplicationBridge");
    NSMutableArray *notificationArray = [[[NSMutableArray alloc] init] autorelease];
    for(std::set<std::string>::iterator itr = notifications.begin(); itr != notifications.end(); ++itr) {
        [notificationArray addObject:[NSString stringWithCString:itr->c_str() encoding:NSUTF8StringEncoding]];
    }
    [GAB registerWithDictionary:[NSDictionary dictionaryWithObjectsAndKeys:
                                                    [NSString stringWithCString:appname.c_str() encoding:NSUTF8StringEncoding], GROWL_APP_NAME,
                                                    notificationArray, GROWL_NOTIFICATIONS_ALL,
                                                    notificationArray, GROWL_NOTIFICATIONS_DEFAULT,
                                                    nil]];
    [pool release];
}
