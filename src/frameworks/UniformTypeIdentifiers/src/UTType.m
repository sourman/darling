/*
 This file is part of Darling.

 Copyright (C) 2023 Darling Team

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

#import <UniformTypeIdentifiers/UTType.h>

@implementation UTType {
	NSString *_identifier;
	NSString *_mimeType;
	NSString *_extension;
}

+ (instancetype)typeWithMIMEType:(NSString *)mimeType
{
	return [self typeWithMIMEType:mimeType conformingToType:nil];
}

+ (instancetype)typeWithMIMEType:(NSString *)mimeType conformingToType:(UTType *)supertype
{
	(void)supertype;
	UTType *type = [[self alloc] init];
	type->_mimeType = [mimeType copy];
	if ([mimeType isEqualToString:@"application/pdf"]) {
		type->_identifier = @"com.adobe.pdf";
		type->_extension = @"pdf";
	} else if ([mimeType isEqualToString:@"text/html"]) {
		type->_identifier = @"public.html";
		type->_extension = @"html";
	} else {
		type->_identifier = mimeType ? [@"public.mime." stringByAppendingString:mimeType] : @"public.data";
	}
	return type;
}

+ (instancetype)typeWithFilenameExtension:(NSString *)filenameExtension
{
	UTType *type = [[self alloc] init];
	type->_extension = [filenameExtension copy];
	if ([filenameExtension isEqualToString:@"pdf"]) {
		type->_identifier = @"com.adobe.pdf";
		type->_mimeType = @"application/pdf";
	} else {
		type->_identifier = filenameExtension ? [@"public.filename-extension." stringByAppendingString:filenameExtension] : @"public.data";
	}
	return type;
}

+ (instancetype)typeWithIdentifier:(NSString *)identifier
{
	UTType *type = [[self alloc] init];
	type->_identifier = [identifier copy];
	return type;
}

- (NSString *)identifier
{
	return _identifier ?: @"public.data";
}

- (NSString *)preferredMIMEType
{
	return _mimeType;
}

- (NSString *)preferredFilenameExtension
{
	return _extension;
}

- (BOOL)conformsToType:(UTType *)type
{
	if (!type)
		return NO;
	return [self.identifier isEqualToString:type.identifier];
}

+ (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
	NSString *name = NSStringFromSelector(aSelector);
	if ([name hasPrefix:@"typeWith"] || [name hasSuffix:@":"])
		return [NSMethodSignature signatureWithObjCTypes:"@@:@"];
	return [NSMethodSignature signatureWithObjCTypes:"@@:"];
}

+ (void)forwardInvocation:(NSInvocation *)anInvocation
{
	NSLog(@"Stub called: +%@ in %@", NSStringFromSelector([anInvocation selector]), self);
	id result = [[self alloc] init];
	[anInvocation setReturnValue:&result];
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
