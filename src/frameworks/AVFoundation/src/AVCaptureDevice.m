/*
 This file is part of Darling.

 Copyright (C) 2017 Lubos Dolezel

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#import <AVFoundation/AVCaptureDevice.h>
#import <objc/runtime.h>
#include <string.h>

@implementation AVCaptureDevice

+ (NSInteger)authorizationStatusForMediaType:(NSString *)mediaType
{
	(void)mediaType;
	/* AVAuthorizationStatusDenied — no camera/mic on this backend. */
	return 2;
}

+ (void)requestAccessForMediaType:(NSString *)mediaType
                completionHandler:(void (^)(BOOL granted))handler
{
	(void)mediaType;
	if (handler)
		handler(NO);
}

+ (NSArray *)devices
{
	return @[];
}

+ (NSArray *)devicesWithMediaType:(NSString *)mediaType
{
	(void)mediaType;
	return @[];
}

+ (id)defaultDeviceWithMediaType:(NSString *)mediaType
{
	(void)mediaType;
	return nil;
}

+ (id)deviceWithUniqueID:(NSString *)deviceUniqueID
{
	(void)deviceUniqueID;
	return nil;
}

+ (id)discoverySessionWithDeviceTypes:(NSArray *)deviceTypes
                            mediaType:(NSString *)mediaType
                             position:(NSInteger)position
{
	(void)deviceTypes;
	(void)mediaType;
	(void)position;
	Class sessionClass = NSClassFromString(@"AVCaptureDeviceDiscoverySession");
	if (sessionClass)
		return [[sessionClass alloc] init];
	return nil;
}

+ (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
	const char *sel = sel_getName(aSelector);
	if (sel && sel[0] != '\0')
	{
		if (strstr(sel, "authorizationStatus") || strstr(sel, "position") || strstr(sel, "Count"))
			return [NSMethodSignature signatureWithObjCTypes: "q@:@"];
		if (strstr(sel, "devices") || strstr(sel, "Device") || strstr(sel, "Session"))
			return [NSMethodSignature signatureWithObjCTypes: "@@:@"];
	}
	return [NSMethodSignature signatureWithObjCTypes: "q@:@"];
}

+ (void)forwardInvocation:(NSInvocation *)anInvocation
{
	NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), self);
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector {
    return [NSMethodSignature signatureWithObjCTypes: "v@:"];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation {
    NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), [self class]);
}

@end
