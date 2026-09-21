/*
 This file is part of Darling.

 Copyright (C) 2020 Lubos Dolezel

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

#import <IOSurface/IOSurface.h>
#import <IOSurface/IOSurfaceObjC.h>
#include <stdio.h>

#define IOS_LOG(fmt, ...) do { \
	fprintf(stderr, "iosurface_sysmem_v1 " fmt "\n", ##__VA_ARGS__); \
	fflush(stderr); \
} while (0)

CFTypeID IOSurfaceGetTypeID(void)
{
	return (CFTypeID) [IOSurface self];
}

IOSurfaceRef _Nullable IOSurfaceCreate(CFDictionaryRef properties)
{
	IOSurfaceRef s = (IOSurfaceRef) [[IOSurface alloc] initWithProperties: (NSDictionary*) properties];
	IOS_LOG("Create -> %p id=%u", s, s ? (unsigned) IOSurfaceGetID(s) : 0);
	return s;
}

IOSurfaceRef _Nullable IOSurfaceLookupFromMachPort(mach_port_t port)
{
	IOS_LOG("LookupFromMachPort port=0x%x", (unsigned) port);
	return (IOSurfaceRef) [[IOSurface alloc] initWithMachPort: port];
}

IOSurfaceRef _Nullable IOSurfaceLookup(IOSurfaceID csid)
{
	IOS_LOG("Lookup id=%u", (unsigned) csid);
	return (IOSurfaceRef) [[IOSurface alloc] initWithSurfaceID: csid];
}

IOSurfaceID IOSurfaceGetID(IOSurfaceRef buffer)
{
	IOSurfaceID id = (IOSurfaceID) [(IOSurface *) buffer hash];
	IOS_LOG("GetID %p -> %u", buffer, (unsigned) id);
	return id;
}

size_t IOSurfaceGetAllocSize(IOSurfaceRef buffer)
{
	size_t n = (size_t) [(IOSurface *) buffer allocationSize];
	IOS_LOG("GetAllocSize %p -> %zu", buffer, n);
	return n;
}

mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef buffer)
{
	mach_port_t p = [(IOSurface *) buffer createMachPort];
	IOS_LOG("CreateMachPort %p -> 0x%x", buffer, (unsigned) p);
	return p;
}

size_t IOSurfaceGetPropertyMaximum(CFStringRef property)
{
	(void) property;
	return 16384;
}

size_t IOSurfaceGetPropertyAlignment(CFStringRef property)
{
	if (property == kIOSurfaceBytesPerRow ||
	    property == kIOSurfaceAllocSize)
		return 64;
	return 16;
}

size_t IOSurfaceAlignProperty(CFStringRef property, size_t value)
{
	size_t a = IOSurfaceGetPropertyAlignment(property);
	if (a < 1)
		a = 1;
	return (value + a - 1) & ~(a - 1);
}

Boolean IOSurfaceAllowsPixelSizeCasting(IOSurfaceRef buffer)
{
	(void) buffer;
	return 1;
}

xpc_object_t IOSurfaceCreateXPCObject(IOSurfaceRef surface)
{
	return 0;
}

IOSurfaceRef IOSurfaceLookupFromXPCObject(xpc_object_t obj)
{
#if NOTYET
	if (xpc_get_type(obj) == _xpc_type_mach_send)
	{
		mach_port_t port = xpc_mach_send_get_right(obj);
		return IOSurfaceLookupFromMachPort(port);
	}
#endif
	return NULL;
}

kern_return_t IOSurfaceLock(IOSurfaceRef buffer, IOSurfaceLockOptions options, uint32_t * _Nullable seed)
{
	kern_return_t r = [(IOSurface*)buffer lockWithOptions: options seed: seed];
	IOS_LOG("Lock %p opts=%u -> %d", buffer, (unsigned) options, (int) r);
	return r;
}

kern_return_t IOSurfaceUnlock(IOSurfaceRef buffer, IOSurfaceLockOptions options, uint32_t * _Nullable seed)
{
	kern_return_t r = [(IOSurface*)buffer unlockWithOptions: options seed: seed];
	IOS_LOG("Unlock %p opts=%u -> %d", buffer, (unsigned) options, (int) r);
	return r;
}

size_t IOSurfaceGetWidth(IOSurfaceRef buffer)
{
	size_t n = [(IOSurface*)buffer width];
	IOS_LOG("GetWidth %p -> %zu", buffer, n);
	return n;
}

size_t IOSurfaceGetHeight(IOSurfaceRef buffer)
{
	size_t n = [(IOSurface*)buffer height];
	IOS_LOG("GetHeight %p -> %zu", buffer, n);
	return n;
}

void *IOSurfaceGetBaseAddress(IOSurfaceRef buffer)
{
	void *p = [(IOSurface*)buffer baseAddress];
	IOS_LOG("GetBaseAddress %p -> %p", buffer, p);
	return p;
}

OSType IOSurfaceGetPixelFormat(IOSurfaceRef buffer)
{
	OSType f = [(IOSurface*)buffer pixelFormat];
	IOS_LOG("GetPixelFormat %p -> 0x%x", buffer, (unsigned) f);
	return f;
}

void IOSurfaceSetValue(IOSurfaceRef buffer, CFStringRef key, CFTypeRef value)
{
	const char *ks = key ? CFStringGetCStringPtr(key, kCFStringEncodingUTF8) : "(null)";
	IOS_LOG("SetValue %p key=%s val=%p", buffer, ks ? ks : "?", value);
	return [(IOSurface*)buffer setAttachment: (id) value forKey: (NSString*) key];
}


CFTypeRef _Nullable IOSurfaceCopyValue(IOSurfaceRef buffer, CFStringRef key)
{
	return (CFTypeRef) [[(IOSurface*) buffer attachmentForKey: (NSString*) key] retain];
}


void IOSurfaceRemoveValue(IOSurfaceRef buffer, CFStringRef key)
{
	[(IOSurface*)buffer removeAttachmentForKey: (NSString*) key];
}


void IOSurfaceSetValues(IOSurfaceRef buffer, CFDictionaryRef keysAndValues)
{
	[(IOSurface*)buffer setAllAttachments: (NSDictionary*)keysAndValues];
}


CFDictionaryRef _Nullable IOSurfaceCopyAllValues(IOSurfaceRef buffer)
{
	return (CFDictionaryRef) [[(IOSurface*)buffer allAttachments] retain];
}


void IOSurfaceRemoveAllValues(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer removeAllAttachments];
}

size_t IOSurfaceGetBytesPerRow(IOSurfaceRef buffer)
{
	size_t n = [(IOSurface*)buffer bytesPerRow];
	IOS_LOG("GetBytesPerRow %p -> %zu", buffer, n);
	return n;
}

void IOSurfaceIncrementUseCount(IOSurfaceRef buffer)
{
	[(IOSurface*)buffer incrementUseCount];
}


void IOSurfaceDecrementUseCount(IOSurfaceRef buffer)
{
	[(IOSurface*)buffer decrementUseCount];
}


int32_t IOSurfaceGetUseCount(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer localUseCount];
}

size_t IOSurfaceGetPlaneCount(IOSurfaceRef buffer)
{
	size_t n = [(IOSurface*)buffer planeCount];
	IOS_LOG("GetPlaneCount %p -> %zu", buffer, n);
	return n;
}

uint32_t IOSurfaceGetSeed(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer seed];
}

size_t IOSurfaceGetWidthOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer widthOfPlaneAtIndex: planeIndex];
}


size_t IOSurfaceGetHeightOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer heightOfPlaneAtIndex: planeIndex];
}


size_t IOSurfaceGetBytesPerElementOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer bytesPerElementOfPlaneAtIndex: planeIndex];
}


size_t IOSurfaceGetBytesPerRowOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer bytesPerRowOfPlaneAtIndex: planeIndex];
}


void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer baseAddressOfPlaneAtIndex: planeIndex];
}


size_t IOSurfaceGetElementWidthOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer elementWidthOfPlaneAtIndex: planeIndex];
}


size_t IOSurfaceGetElementHeightOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	return [(IOSurface*)buffer elementHeightOfPlaneAtIndex: planeIndex];
}

size_t IOSurfaceGetBytesPerElement(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer bytesPerElementOfPlaneAtIndex: 0];
}


size_t IOSurfaceGetElementWidth(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer elementWidth];
}


size_t IOSurfaceGetElementHeight(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer elementHeight];
}

Boolean IOSurfaceIsInUse(IOSurfaceRef buffer)
{
	return [(IOSurface*)buffer isInUse];
}

kern_return_t IOSurfaceSetPurgeable(IOSurfaceRef buffer, uint32_t newState, uint32_t * _Nullable oldState)
{
	return [(IOSurface*)buffer setPurgeable:newState oldState:oldState];
}
