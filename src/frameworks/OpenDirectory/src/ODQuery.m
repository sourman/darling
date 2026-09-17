/*
 This file is part of Darling.

 Copyright (C) 2019 Lubos Dolezel

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

#import <OpenDirectory/ODQuery.h>
#import <OpenDirectory/ODNode.h>

#import <Foundation/Foundation.h>

@implementation ODQuery

+ (ODQuery *)queryWithNode:(ODNode *)inNode forRecordTypes:(id)inRecordTypeOrList attribute:(ODAttributeType)inAttribute
                 matchType:(ODMatchType)inMatchType queryValues:(id)inQueryValueOrList
          returnAttributes:(id)inReturnAttributeOrList maximumResults:(NSInteger)inMaximumResults
                     error:(NSError **)outError
{
	if (outError)
		*outError = nil;
	return [[[self alloc] init] autorelease];
}

- (instancetype)initWithNode:(ODNode *)inNode forRecordTypes:(id)inRecordTypeOrList attribute:(ODAttributeType)inAttribute
                   matchType:(ODMatchType)inMatchType queryValues:(id)inQueryValueOrList
            returnAttributes:(id)inReturnAttributeOrList maximumResults:(NSInteger)inMaximumResults
                       error:(NSError **)outError
{
	if (outError)
		*outError = nil;
	return [self init];
}

- (NSArray *)resultsAllowingPartial:(BOOL)inAllowPartialResults error:(NSError **)outError
{
	if (outError)
		*outError = nil;
	return [NSArray array];
}

- (void)scheduleInRunLoop:(NSRunLoop *)inRunLoop forMode:(NSString *)inMode
{
}

- (void)removeFromRunLoop:(NSRunLoop *)inRunLoop forMode:(NSString *)inMode
{
}

- (void)synchronize
{
}

- (id)copyWithZone:(NSZone *)zone
{
	return [self retain];
}

+ (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
	return [NSMethodSignature signatureWithObjCTypes: "@@:"];
}

+ (void)forwardInvocation:(NSInvocation *)anInvocation
{
	NSLog(@"Stub called: +%@ in %@", NSStringFromSelector([anInvocation selector]), self);
	id ret = nil;
	[anInvocation setReturnValue:&ret];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
	return [NSMethodSignature signatureWithObjCTypes: "@@:"];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
	NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), [self class]);
	id ret = nil;
	[anInvocation setReturnValue:&ret];
}

@end
