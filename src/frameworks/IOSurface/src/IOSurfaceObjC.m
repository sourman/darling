/*
 This file is part of Darling.

 Copyright (C) 2020 Lubos Dolezel

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
*/

#import <IOSurface/IOSurface.h>
#import <IOSurface/IOSurfaceObjC.h>
#import <IOSurface/IOSurfacePriv.h>
#import <Foundation/Foundation.h>
#import <mach/mach.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define kPixelBGRA 'BGRA'
#define IOSURFACE_ROW_ALIGN 64

__attribute__((used)) const char iosurface_sysmem_v1[] = "iosurface_sysmem_v1";
__attribute__((used)) const char iosurface_props_v2[] = "iosurface_props_v2";

typedef struct
{
	struct _IOSurfaceObjectRetval *surface;
	void *memory;
	size_t allocSize;
	_Atomic int32_t localUseCount;
	uint32_t seed;
	int purgeable;
	NSMutableDictionary *attachments;
	mach_port_t mach_port;
	uint32_t cacheMode;
} ImplData;

static pthread_mutex_t g_idLock = PTHREAD_MUTEX_INITIALIZER;
static NSMutableDictionary *g_byID;
static NSMutableDictionary *g_byPort;
static IOSurfaceID g_nextID = 1;

static size_t align_up(size_t v, size_t a)
{
	return (v + a - 1) & ~(a - 1);
}

static int int_prop(NSDictionary *properties, NSString *key, NSString *alt, int fallback)
{
	id v = properties[key];
	if (!v && alt)
		v = properties[alt];
	if ([v isKindOfClass: [NSNumber class]])
		return [v intValue];
	return fallback;
}

static uint32_t u32_prop(NSDictionary *properties, NSString *key, NSString *alt, uint32_t fallback)
{
	id v = properties[key];
	if (!v && alt)
		v = properties[alt];
	if ([v isKindOfClass: [NSNumber class]])
		return [v unsignedIntValue];
	return fallback;
}

/* Apple: IOSurfaceGetPlaneCount is 0 for non-planar (single-plane) surfaces,
   but plane-0 getters still return the buffer geometry. */
static int plane_ok(ImplData *data, NSUInteger planeIndex)
{
	uint32_t n = data->surface->planeCount;
	if (n <= 1)
		return planeIndex == 0;
	return planeIndex < n;
}

@implementation IOSurface

+ (void)initialize
{
	if (self == [IOSurface self]) {
		g_byID = [[NSMutableDictionary alloc] init];
		g_byPort = [[NSMutableDictionary alloc] init];
	}
}

- (nullable instancetype)initWithResponse:(const void*)bytes length:(size_t)length
{
	ImplData* idata = (ImplData*) calloc(1, sizeof(ImplData));
	idata->surface = malloc(length);
	memcpy(idata->surface, bytes, length);
	idata->localUseCount = 0;
	idata->attachments = [[NSMutableDictionary alloc] init];
	self->_impl = idata;
	return self;
}

- (nullable instancetype)initWithProperties:(NSDictionary <IOSurfacePropertyKey, id> *)properties
{
	int width = int_prop(properties, (NSString *) kIOSurfaceWidth, @"IOSurfaceWidth", 0);
	int height = int_prop(properties, (NSString *) kIOSurfaceHeight, @"IOSurfaceHeight", 0);
	int bpe = int_prop(properties, (NSString *) kIOSurfaceBytesPerElement, @"IOSurfaceBytesPerElement", 4);
	int bpr = int_prop(properties, (NSString *) kIOSurfaceBytesPerRow, @"IOSurfaceBytesPerRow", 0);
	uint32_t pixelFormat = u32_prop(properties, (NSString *) kIOSurfacePixelFormat,
					@"IOSurfacePixelFormat", (uint32_t) kPixelBGRA);
	int allocHint = int_prop(properties, (NSString *) kIOSurfaceAllocSize, @"IOSurfaceAllocSize", 0);

	if (width <= 0 || height <= 0) {
		fprintf(stderr, "iosurface_sysmem_v1 create fail w=%d h=%d\n", width, height);
		fflush(stderr);
		[self release];
		return nil;
	}
	if (bpe <= 0)
		bpe = 4;
	if (pixelFormat == 0)
		pixelFormat = kPixelBGRA;
	if (bpr <= 0)
		bpr = (int) align_up((size_t) width * (size_t) bpe, IOSURFACE_ROW_ALIGN);
	size_t allocSize = (size_t) bpr * (size_t) height;
	if (allocHint > 0 && (size_t) allocHint > allocSize)
		allocSize = (size_t) allocHint;
	allocSize = align_up(allocSize, 4096);

	void *memory = NULL;
	if (posix_memalign(&memory, 4096, allocSize) != 0 || !memory) {
		[self release];
		return nil;
	}
	memset(memory, 0, allocSize);

	ImplData *idata = (ImplData *) calloc(1, sizeof(ImplData));
	struct _IOSurfaceObjectRetval *rv = (struct _IOSurfaceObjectRetval *) calloc(1, sizeof(*rv));
	pthread_mutex_lock(&g_idLock);
	rv->surfaceID = g_nextID++;
	pthread_mutex_unlock(&g_idLock);
	rv->pixelFormat = pixelFormat;
	rv->address = (uint64_t)(uintptr_t) memory;
	rv->planeCount = 1;
	rv->planes[0].memoryOffset = 0;
	rv->planes[0].bytesPerElement = (uint32_t) bpe;
	rv->planes[0].width = (uint32_t) width;
	rv->planes[0].height = (uint32_t) height;
	rv->planes[0].bytesPerRow = (uint32_t) bpr;
	idata->surface = rv;
	idata->memory = memory;
	idata->allocSize = allocSize;
	idata->localUseCount = 0;
	idata->seed = 1;
	idata->purgeable = kIOSurfacePurgeableNonVolatile;
	idata->attachments = [[NSMutableDictionary alloc] init];
	idata->mach_port = MACH_PORT_NULL;
	idata->cacheMode = (uint32_t) int_prop(properties, (NSString *) kIOSurfaceCacheMode,
					       @"IOSurfaceCacheMode", kIOSurfaceDefaultCache);
	/* Seed keys Chrome reads immediately after IOSurfaceCreate. */
	[idata->attachments setObject: (id) CFSTR("kCGColorSpaceSRGB")
				forKey: @"IOSurfaceColorSpace"];
	[idata->attachments setObject: @YES forKey: @"IsDisplayable"];
	self->_impl = idata;

	pthread_mutex_lock(&g_idLock);
	if (!g_byID)
		g_byID = [[NSMutableDictionary alloc] init];
	[g_byID setObject: self forKey: @(rv->surfaceID)];
	pthread_mutex_unlock(&g_idLock);

	fprintf(stderr,
		"iosurface_sysmem_v1 create id=%u %dx%d bpr=%d bpe=%d fmt=0x%x alloc=%zu ptr=%p\n",
		(unsigned) rv->surfaceID, width, height, bpr, bpe, pixelFormat, allocSize, memory);
	fflush(stderr);
	return self;
}

- (nullable instancetype)initWithSurfaceID:(IOSurfaceID)surfaceID
{
	pthread_mutex_lock(&g_idLock);
	IOSurface *existing = [[g_byID objectForKey: @(surfaceID)] retain];
	pthread_mutex_unlock(&g_idLock);
	[self release];
	return existing;
}

- (nullable instancetype)initWithMachPort:(mach_port_t)port
{
	pthread_mutex_lock(&g_idLock);
	IOSurface *existing = [[g_byPort objectForKey: @(port)] retain];
	if (!existing)
		existing = [[g_byID objectForKey: @(port)] retain];
	pthread_mutex_unlock(&g_idLock);
	[self release];
	return existing;
}

- (void) dealloc
{
	ImplData* data = (ImplData*) _impl;
	if (data != NULL)
	{
		if (data->surface != NULL)
		{
			pthread_mutex_lock(&g_idLock);
			[g_byID removeObjectForKey: @(data->surface->surfaceID)];
			if (data->mach_port != MACH_PORT_NULL)
				[g_byPort removeObjectForKey: @(data->mach_port)];
			pthread_mutex_unlock(&g_idLock);
			free(data->surface);
			data->surface = NULL;
		}
		if (data->mach_port != MACH_PORT_NULL)
			mach_port_deallocate(mach_task_self(), data->mach_port);
		free(data->memory);
		[data->attachments release];
		free(data);
	}
	[super dealloc];
}

- (NSUInteger) hash
{
	ImplData* data = (ImplData*) _impl;
	return data->surface->surfaceID;
}

- (BOOL)isEqual:(id)object
{
	if (![object isKindOfClass: [self class]])
		return NO;

	IOSurface* that = (IOSurface*) object;
	return ((ImplData*) self->_impl)->surface->surfaceID == ((ImplData*) that->_impl)->surface->surfaceID;
}

- (CFTypeID) _cfTypeID
{
    return IOSurfaceGetTypeID();
}

- (NSInteger) allocationSize
{
	return (NSInteger) ((ImplData*) _impl)->allocSize;
}

- (uint32_t) seed
{
	return ((ImplData*) _impl)->seed;
}

- (NSInteger) width
{
	return [self widthOfPlaneAtIndex: 0];
}

- (NSInteger) height
{
	return [self heightOfPlaneAtIndex: 0];
}

- (void *) baseAddress
{
	return [self baseAddressOfPlaneAtIndex: 0];
}

- (NSInteger) bytesPerRow
{
	return [self bytesPerRowOfPlaneAtIndex: 0];
}

- (NSInteger) bytesPerElement
{
	return [self bytesPerElementOfPlaneAtIndex: 0];
}

- (NSInteger) elementWidth
{
	return [self elementWidthOfPlaneAtIndex: 0];
}

- (NSInteger) elementHeight
{
	return [self elementHeightOfPlaneAtIndex: 0];
}

- (OSType) pixelFormat
{
	return ((ImplData*) _impl)->surface->pixelFormat;
}

- (NSUInteger) planeCount
{
	/* Apple returns 0 for non-planar (single-plane) IOSurfaces. */
	uint32_t n = ((ImplData*) _impl)->surface->planeCount;
	return n <= 1 ? 0 : n;
}

- (NSInteger)widthOfPlaneAtIndex:(NSUInteger)planeIndex
{
	ImplData* data = (ImplData*) _impl;
	if (!plane_ok(data, planeIndex))
		return 0;
	return data->surface->planes[planeIndex].width;
}

- (NSInteger)heightOfPlaneAtIndex:(NSUInteger)planeIndex
{
	ImplData* data = (ImplData*) _impl;
	if (!plane_ok(data, planeIndex))
		return 0;
	return data->surface->planes[planeIndex].height;
}

- (NSInteger)bytesPerRowOfPlaneAtIndex:(NSUInteger)planeIndex
{
	ImplData* data = (ImplData*) _impl;
	if (!plane_ok(data, planeIndex))
		return 0;
	return data->surface->planes[planeIndex].bytesPerRow;
}

- (NSInteger)bytesPerElementOfPlaneAtIndex:(NSUInteger)planeIndex
{
	ImplData* data = (ImplData*) _impl;
	if (!plane_ok(data, planeIndex))
		return 0;
	return data->surface->planes[planeIndex].bytesPerElement;
}

- (NSInteger)elementWidthOfPlaneAtIndex:(NSUInteger)planeIndex
{
	(void) planeIndex;
	return 1;
}

- (NSInteger)elementHeightOfPlaneAtIndex:(NSUInteger)planeIndex
{
	(void) planeIndex;
	return 1;
}

- (void *)baseAddressOfPlaneAtIndex:(NSUInteger)planeIndex
{
	ImplData* data = (ImplData*) _impl;
	if (!plane_ok(data, planeIndex))
		return NULL;
	return (void*)((uintptr_t) data->memory + data->surface->planes[planeIndex].memoryOffset);
}

- (void)incrementUseCount
{
	ImplData* data = (ImplData*) _impl;
	atomic_fetch_add(&data->localUseCount, 1);
}

- (void)decrementUseCount
{
	ImplData* data = (ImplData*) _impl;
	atomic_fetch_sub(&data->localUseCount, 1);
}

- (int32_t) localUseCount
{
	ImplData* data = (ImplData*) _impl;
	return atomic_load(&data->localUseCount);
}

- (BOOL) isInUse
{
	return [self localUseCount] > 0;
}

- (BOOL) allowsPixelSizeCasting
{
	return YES;
}

- (kern_return_t)lockWithOptions:(IOSurfaceLockOptions)options seed:(nullable uint32_t *)seed
{
	ImplData* data = (ImplData*) _impl;
	(void) options;
	if (seed)
		*seed = data->seed;
	return kIOSurfaceSuccess;
}

- (kern_return_t)unlockWithOptions:(IOSurfaceLockOptions)options seed:(nullable uint32_t *)seed
{
	ImplData* data = (ImplData*) _impl;
	(void) options;
	data->seed++;
	if (seed)
		*seed = data->seed;
	return kIOSurfaceSuccess;
}

- (void)setAttachment:(id)anObject forKey:(NSString *)key
{
	ImplData* data = (ImplData*) _impl;
	if (key && anObject)
		[data->attachments setObject: anObject forKey: key];
}

- (nullable id)attachmentForKey:(NSString *)key
{
	ImplData* data = (ImplData*) _impl;
	if (!key)
		return nil;
	return [data->attachments objectForKey: key];
}

- (void)removeAttachmentForKey:(NSString *)key
{
	ImplData* data = (ImplData*) _impl;
	if (key)
		[data->attachments removeObjectForKey: key];
}

- (void)setAllAttachments:(NSDictionary<NSString *, id> *)dict
{
	ImplData* data = (ImplData*) _impl;
	[data->attachments removeAllObjects];
	if (dict)
		[data->attachments addEntriesFromDictionary: dict];
}

- (nullable NSDictionary<NSString *, id> *)allAttachments
{
	ImplData* data = (ImplData*) _impl;
	return [[data->attachments copy] autorelease];
}

- (void)removeAllAttachments
{
	ImplData* data = (ImplData*) _impl;
	[data->attachments removeAllObjects];
}

- (kern_return_t)setPurgeable:(IOSurfacePurgeabilityState)newState oldState:(IOSurfacePurgeabilityState * _Nullable)oldState
{
	ImplData* data = (ImplData*) _impl;
	if (oldState)
		*oldState = (IOSurfacePurgeabilityState) data->purgeable;
	if (newState != kIOSurfacePurgeableKeepCurrent)
		data->purgeable = (int) newState;
	return kIOSurfaceSuccess;
}

- (mach_port_t) createMachPort
{
	ImplData* data = (ImplData*) _impl;
	if (data->mach_port == MACH_PORT_NULL) {
		mach_port_t p = MACH_PORT_NULL;
		kern_return_t kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &p);
		if (kr == KERN_SUCCESS) {
			mach_port_insert_right(mach_task_self(), p, p, MACH_MSG_TYPE_MAKE_SEND);
			data->mach_port = p;
			pthread_mutex_lock(&g_idLock);
			if (!g_byPort)
				g_byPort = [[NSMutableDictionary alloc] init];
			[g_byPort setObject: self forKey: @(p)];
			pthread_mutex_unlock(&g_idLock);
		} else {
			data->mach_port = (mach_port_t) data->surface->surfaceID;
		}
		fprintf(stderr, "iosurface_sysmem_v1 machport id=%u port=0x%x kr=%d\n",
			(unsigned) data->surface->surfaceID, (unsigned) data->mach_port, (int) kr);
		fflush(stderr);
	}
	return data->mach_port;
}

+ (BOOL)supportsSecureCoding
{
	return YES;
}

- (nullable instancetype)initWithCoder:(NSCoder *)coder
{
	IOSurfaceID sid = (IOSurfaceID) [coder decodeInt32ForKey: @"IOSurfaceID"];
	return [self initWithSurfaceID: sid];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
	ImplData* data = (ImplData*) _impl;
	[coder encodeInt32: (int32_t) data->surface->surfaceID forKey: @"IOSurfaceID"];
}

- (NSString *)description
{
	ImplData* data = (ImplData*) _impl;
	return [NSString stringWithFormat: @"<IOSurface id=%u %dx%d fmt=0x%x bpr=%u ptr=%p>",
		(unsigned) data->surface->surfaceID,
		(int) data->surface->planes[0].width,
		(int) data->surface->planes[0].height,
		(unsigned) data->surface->pixelFormat,
		(unsigned) data->surface->planes[0].bytesPerRow,
		data->memory];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector {
    return [NSMethodSignature signatureWithObjCTypes: "v@:"];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation {
    NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), [self class]);
}

@end
