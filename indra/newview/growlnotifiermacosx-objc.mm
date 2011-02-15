/* Copyright (c) 2010 Katharine Berry All rights reserved.
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

// Make a happy delegate (that does nothing)
@interface FirestormBridge : NSObject <GrowlApplicationBridgeDelegate>
@end

@implementation FirestormBridge
@end

void growlApplicationBridgeNotify(const std::string& withTitle, const std::string& description, const std::string& notificationName, 
                                void *iconData, unsigned int iconDataSize, int priority, bool isSticky) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [GrowlApplicationBridge notifyWithTitle:[NSString stringWithCString:withTitle.c_str() encoding:NSUTF8StringEncoding]
                                description:[NSString stringWithCString:description.c_str() encoding:NSUTF8StringEncoding]
                           notificationName:[NSString stringWithCString:notificationName.c_str() encoding:NSUTF8StringEncoding]
                                   iconData:(iconData == NULL ? nil : [NSData dataWithBytes:iconData length:iconDataSize])
                                   priority:priority
                                   isSticky:isSticky
                               clickContext:nil
     ];
    [pool release];
}

void growlApplicationBridgeInit() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    FirestormBridge *delegate = [[[FirestormBridge alloc] init] autorelease];
    [GrowlApplicationBridge setGrowlDelegate:delegate];
    [pool release];
}

bool growlApplicationBridgeIsGrowlInstalled() {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    bool installed = [GrowlApplicationBridge isGrowlInstalled];
    [pool release];
    return installed;
}

void growlApplicationBridgeRegister(const std::string& appname, const std::set<std::string>& notifications)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSMutableArray *notificationArray = [[[NSMutableArray alloc] init] autorelease];
    for(std::set<std::string>::iterator itr = notifications.begin(); itr != notifications.end(); ++itr) {
        [notificationArray addObject:[NSString stringWithCString:itr->c_str() encoding:NSUTF8StringEncoding]];
    }
    [GrowlApplicationBridge registerWithDictionary:[NSDictionary dictionaryWithObjectsAndKeys:
                                                    [NSString stringWithCString:appname.c_str() encoding:NSUTF8StringEncoding], GROWL_APP_NAME,
                                                    notificationArray, GROWL_NOTIFICATIONS_ALL,
                                                    notificationArray, GROWL_NOTIFICATIONS_DEFAULT,
                                                    nil]];
    [pool release];
}