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

#import <OpenDirectory/ODNode.h>
#import <OpenDirectory/ODRecord.h>

@implementation ODNode

+ (instancetype)nodeWithSession:(ODSession *)inSession type:(ODNodeType)inType error:(NSError **)outError
{
    (void)inSession;
    (void)inType;
    if (outError != NULL)
        *outError = nil;
    return [[[self alloc] init] autorelease];
}

+ (instancetype)nodeWithSession:(ODSession *)inSession name:(NSString *)inName error:(NSError **)outError
{
    (void)inSession;
    (void)inName;
    if (outError != NULL)
        *outError = nil;
    return [[[self alloc] init] autorelease];
}

- (ODRecord *)recordWithRecordType:(ODRecordType)inRecordType name:(NSString *)inRecordName attributes:(id)inAttributes error:(NSError **)outError
{
    (void)inRecordType;
    (void)inRecordName;
    (void)inAttributes;
    if (outError != NULL)
        *outError = nil;
    return [[[ODRecord alloc] init] autorelease];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    return [NSMethodSignature signatureWithObjCTypes: "v@:"];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), [self class]);
}

@end
