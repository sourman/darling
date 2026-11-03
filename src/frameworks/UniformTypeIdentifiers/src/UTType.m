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
    NSString *_filenameExtension;
}

static UTType *UTTypeHTMLType;
static UTType *UTTypePlainTextType;
static UTType *UTTypePDFType;
static UTType *UTTypeTextType;
static UTType *UTTypeDataType;

static UTType *UTTypeCreate(NSString *identifier, NSString *mimeType, NSString *filenameExtension) {
    UTType *type = [[UTType alloc] init];
    type->_identifier = [identifier copy];
    type->_mimeType = [mimeType copy];
    type->_filenameExtension = [filenameExtension copy];
    return type;
}

+ (void)initialize {
    if (self != [UTType class])
        return;
    UTTypeHTMLType = UTTypeCreate(@"public.html", @"text/html", @"html");
    UTTypePlainTextType = UTTypeCreate(@"public.plain-text", @"text/plain", @"txt");
    UTTypePDFType = UTTypeCreate(@"com.adobe.pdf", @"application/pdf", @"pdf");
    UTTypeTextType = UTTypeCreate(@"public.text", @"text/plain", @"txt");
    UTTypeDataType = UTTypeCreate(@"public.data", nil, nil);
}

+ (UTType *)typeWithMIMEType:(NSString *)mimeType {
    if ([mimeType isEqualToString:@"text/html"])
        return UTTypeHTMLType;
    if ([mimeType isEqualToString:@"text/plain"])
        return UTTypePlainTextType;
    if ([mimeType isEqualToString:@"application/pdf"])
        return UTTypePDFType;
    return UTTypeCreate(@"public.data", mimeType, nil);
}

- (NSString *)identifier {
    return _identifier;
}

- (NSString *)preferredFilenameExtension {
    return _filenameExtension;
}

- (BOOL)isDynamic {
    return NO;
}

- (NSString *)preferredMIMEType {
    return _mimeType;
}

- (BOOL)conformsToType:(UTType *)type {
    if (type == nil)
        return NO;
    if (type == self)
        return YES;
    if (![type isKindOfClass:[UTType class]])
        return NO;
    if ([_identifier isEqualToString:type->_identifier])
        return YES;
    if (type == UTTypeDataType || [type->_identifier isEqualToString:@"public.data"])
        return YES;
    if ((type == UTTypeTextType || [type->_identifier isEqualToString:@"public.text"]) &&
        ([_identifier isEqualToString:@"public.html"] ||
         [_identifier isEqualToString:@"public.plain-text"] ||
         [_identifier isEqualToString:@"public.text"]))
        return YES;
    return NO;
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
