/*
 This file is part of Darling.

 Copyright (C) 2022 Darling Team

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

#import <AuthenticationServices/ASWebAuthenticationSessionWebBrowserSessionManager.h>
#include <objc/runtime.h>
#include <stdio.h>

const char asweb_shared_manager_v1[] = "asweb_shared_manager_v1";

@implementation ASWebAuthenticationSessionWebBrowserSessionManager {
	id _sessionHandler;
}

+ (instancetype)sharedManager
{
	static ASWebAuthenticationSessionWebBrowserSessionManager *shared = nil;
	if (shared == nil)
		shared = [[self alloc] init];
	fprintf(stderr, "asweb_shared_manager_v1 shared=%p class=%s\n",
		shared, class_getName([shared class]));
	fflush(stderr);
	return shared;
}

- (id)sessionHandler
{
	return _sessionHandler;
}

- (void)setSessionHandler:(id)handler
{
	_sessionHandler = handler;
	fprintf(stderr, "asweb_shared_manager_v1 setSessionHandler=%p\n", handler);
	fflush(stderr);
}

- (BOOL)wasLaunchedByAuthenticationServices
{
	return NO;
}

- (void)didFinishLaunch
{
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
	NSMethodSignature *sig = [super methodSignatureForSelector: aSelector];
	if (sig)
		return sig;
	return [NSMethodSignature signatureWithObjCTypes: "v@:"];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
	NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), [self class]);
}

+ (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
	NSMethodSignature *sig = [super methodSignatureForSelector: aSelector];
	if (sig)
		return sig;
	if (sel_isEqual(aSelector, @selector(sharedManager)))
		return [NSMethodSignature signatureWithObjCTypes: "@@:"];
	return [NSMethodSignature signatureWithObjCTypes: "v@:"];
}

+ (void)forwardInvocation:(NSInvocation *)anInvocation
{
	if (sel_isEqual([anInvocation selector], @selector(sharedManager))) {
		id shared = [self sharedManager];
		[anInvocation setReturnValue: &shared];
		return;
	}
	NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), self);
}

@end
