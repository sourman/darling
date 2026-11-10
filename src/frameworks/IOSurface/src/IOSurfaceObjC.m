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
#import <IOSurface/IOSurfacePriv.h>
#import <Foundation/Foundation.h>
#import <IOKit/IOCFSerialize.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

static io_service_t g_surfaceService;

typedef struct
{
	struct _IOSurfaceObjectRetval* surface;
	_Atomic int32_t localUseCount;
	int shmfd;
	size_t mapLength;
	void* mapBase;
} ImplData;

/* Shared with every process in the prefix. The mach port handed out by
   IOSurfaceCreateMachPort is the surface id, so IOSurfaceLookupFromMachPort
   opens the same object. */
static uint32_t darling_surface_id(void)
{
	static int fd = -1;
	static uint32_t* counter = NULL;

	if (counter == NULL)
	{
		fd = open("/tmp/darling-iosurface-seq", O_CREAT | O_RDWR, 0666);
		if (fd < 0)
			return 0;
		if (ftruncate(fd, sizeof(uint32_t)) != 0)
			return 0;
		counter = mmap(NULL, sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (counter == MAP_FAILED)
		{
			counter = NULL;
			return 0;
		}
	}
	return __sync_add_and_fetch(counter, 1);
}

static uint32_t dict_u32(CFDictionaryRef dict, CFStringRef key)
{
	CFTypeRef value;
	int64_t number = 0;

	if (dict == NULL)
		return 0;
	value = CFDictionaryGetValue(dict, key);
	if (value == NULL || CFGetTypeID(value) != CFNumberGetTypeID())
		return 0;
	CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &number);
	if (number < 0)
		return 0;
	return (uint32_t)number;
}

static IOSurface* surface_from_shm(uint32_t surfaceID)
{
	char name[64];
	int fd;
	struct {
		uint32_t magic;
		uint32_t id;
		uint32_t width;
		uint32_t height;
		uint32_t bytesPerRow;
		uint32_t bytesPerElement;
		uint32_t pixelFormat;
		uint32_t allocSize;
	} header;
	void* mapping;
	size_t mapLength;
	struct _IOSurfaceObjectRetval* response;
	IOSurface* surface;
	ImplData* impl;

	snprintf(name, sizeof(name), "/tmp/darling-iosurf-%u", surfaceID);
	fd = open(name, O_RDWR, 0666);
	if (fd < 0)
		return nil;

	if (pread(fd, &header, sizeof(header), 0) != (ssize_t)sizeof(header) || header.magic != 0x46534F49 || header.id != surfaceID)
	{
		close(fd);
		return nil;
	}
	mapLength = 4096u + header.allocSize;
	mapping = mmap(NULL, mapLength, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapping == MAP_FAILED)
	{
		close(fd);
		return nil;
	}

	response = calloc(1, sizeof(*response));
	response->surfaceID = surfaceID;
	response->pixelFormat = header.pixelFormat;
	response->address = (uint64_t)((uint8_t*)mapping + 4096);
	response->planeCount = 1;
	response->planes[0].memoryOffset = 0;
	response->planes[0].bytesPerElement = header.bytesPerElement;
	response->planes[0].width = header.width;
	response->planes[0].height = header.height;
	response->planes[0].bytesPerRow = header.bytesPerRow;

	surface = [[IOSurface alloc] initWithResponse: response length: sizeof(*response)];
	impl = (ImplData*)surface->_impl;
	impl->shmfd = fd;
	impl->mapLength = mapLength;
	impl->mapBase = mapping;
	free(response);
	return surface;
}

@implementation IOSurface

+ (void)initialize
{
	if (self == [IOSurface self])
	{
		CFMutableDictionaryRef matching = IOServiceNameMatching("IOSurfaceRoot");

		/* IOServiceGetMatchingService releases the matching dictionary. */
		g_surfaceService = IOServiceGetMatchingService(kIOMasterPortDefault, matching);

		if (!g_surfaceService)
			NSLog(@"Cannot obtain IOSurfaceRoot!\n");
	}
}

- (nullable instancetype)initWithMachPort:(mach_port_t)port
{
	IOSurface* found;

	mach_port_context_t context = 0;

	if (port == MACH_PORT_NULL)
	{
		[self release];
		return nil;
	}
	if (mach_port_get_context(mach_task_self(), port, &context) == KERN_SUCCESS && context != 0)
		found = surface_from_shm((uint32_t)context);
	else
		found = surface_from_shm((uint32_t)port);
	[self release];
	return found;
}

- (nullable instancetype)initWithResponse:(const void*)bytes length:(size_t)length
{
	ImplData* idata = (ImplData*) calloc(1, sizeof(ImplData));
	idata->surface = malloc(length);
	idata->shmfd = -1;
	memcpy(idata->surface, bytes, length);

	idata->localUseCount = 0;
	self->_impl = idata;

	return self;
}

- (nullable instancetype)initWithProperties:(NSDictionary <IOSurfacePropertyKey, id> *)properties
{
	CFDictionaryRef dict = (CFDictionaryRef)properties;
	uint32_t width = dict_u32(dict, kIOSurfaceWidth);
	uint32_t height = dict_u32(dict, kIOSurfaceHeight);
	uint32_t bytesPerElement = dict_u32(dict, kIOSurfaceBytesPerElement);
	uint32_t bytesPerRow = dict_u32(dict, kIOSurfaceBytesPerRow);
	uint32_t pixelFormat = dict_u32(dict, kIOSurfacePixelFormat);
	uint32_t allocSize = dict_u32(dict, kIOSurfaceAllocSize);
	uint32_t surfaceID;
	char name[64];
	int fd;
	size_t mapLength;
	void* mapping;
	struct {
		uint32_t magic;
		uint32_t id;
		uint32_t width;
		uint32_t height;
		uint32_t bytesPerRow;
		uint32_t bytesPerElement;
		uint32_t pixelFormat;
		uint32_t allocSize;
	} header;
	struct _IOSurfaceObjectRetval* response;
	IOSurface* created;
	ImplData* impl;

	if (bytesPerElement == 0)
		bytesPerElement = 4;
	if (bytesPerRow == 0 && width != 0)
		bytesPerRow = (width * bytesPerElement + 15u) & ~15u;
	if (allocSize == 0 && bytesPerRow != 0 && height != 0)
		allocSize = bytesPerRow * height;
	if (width == 0 || height == 0 || bytesPerRow == 0 || allocSize == 0)
	{
		fprintf(stderr, "IOSurfaceCreate fail=bad-size w=%u h=%u bpe=%u bpr=%u alloc=%u errno=%d\n",
			width, height, bytesPerElement, bytesPerRow, allocSize, errno);
		fflush(stderr);
		[self release];
		return nil;
	}

	surfaceID = darling_surface_id();
	if (surfaceID == 0)
	{
		fprintf(stderr, "IOSurfaceCreate fail=id w=%u h=%u errno=%d\n", width, height, errno);
		fflush(stderr);
		[self release];
		return nil;
	}
	snprintf(name, sizeof(name), "/tmp/darling-iosurf-%u", surfaceID);
	fd = open(name, O_CREAT | O_RDWR, 0666);
	if (fd < 0)
	{
		fprintf(stderr, "IOSurfaceCreate fail=shm_open name=%s errno=%d\n", name, errno);
		fflush(stderr);
		[self release];
		return nil;
	}
	mapLength = 4096u + allocSize;
	if (ftruncate(fd, (off_t)mapLength) != 0)
	{
		fprintf(stderr, "IOSurfaceCreate fail=ftruncate len=%zu errno=%d\n", mapLength, errno);
		fflush(stderr);
		close(fd);
		unlink(name);
		[self release];
		return nil;
	}
	mapping = mmap(NULL, mapLength, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapping == MAP_FAILED)
	{
		fprintf(stderr, "IOSurfaceCreate fail=mmap len=%zu errno=%d\n", mapLength, errno);
		fflush(stderr);
		close(fd);
		unlink(name);
		[self release];
		return nil;
	}
	memset(&header, 0, sizeof(header));
	header.magic = 0x46534F49;
	header.id = surfaceID;
	header.width = width;
	header.height = height;
	header.bytesPerRow = bytesPerRow;
	header.bytesPerElement = bytesPerElement;
	header.pixelFormat = pixelFormat;
	header.allocSize = allocSize;
	memcpy(mapping, &header, sizeof(header));

	response = calloc(1, sizeof(*response));
	response->surfaceID = surfaceID;
	response->pixelFormat = pixelFormat;
	response->address = (uint64_t)((uint8_t*)mapping + 4096);
	response->planeCount = 1;
	response->planes[0].bytesPerElement = bytesPerElement;
	response->planes[0].width = width;
	response->planes[0].height = height;
	response->planes[0].bytesPerRow = bytesPerRow;

	created = [self initWithResponse: response length: sizeof(*response)];
	free(response);
	if (created == nil)
	{
		fprintf(stderr, "IOSurfaceCreate fail=response id=%u errno=%d\n", surfaceID, errno);
		fflush(stderr);
		munmap(mapping, mapLength);
		close(fd);
		unlink(name);
		return nil;
	}
	fprintf(stderr, "IOSurfaceCreate ok id=%u %ux%u alloc=%u\n", surfaceID, width, height, allocSize);
	fflush(stderr);
	impl = (ImplData*)created->_impl;
	impl->shmfd = fd;
	impl->mapLength = mapLength;
	impl->mapBase = mapping;
	return created;
}

- (nullable instancetype)initWithSurfaceID:(IOSurfaceID)surfaceID
{
	IOSurface* found = surface_from_shm(surfaceID);
	uint8_t responseBuffer[3500];
	size_t responseLength = sizeof(responseBuffer);

	if (found != nil)
	{
		[self release];
		return found;
	}

	uint64_t scalar = surfaceID;
	kern_return_t ret = IOConnectCallMethod(g_surfaceService, kIOSurfaceMethodLookupByID, &scalar, 1, NULL, 0, NULL, 0, responseBuffer, &responseLength);

	if (ret != kIOSurfaceSuccess)
	{
		[self release];
		return nil;
	}

	return [self initWithResponse: responseBuffer length: responseLength];
}

- (void) dealloc
{
	ImplData* data = (ImplData*) _impl;
	if (data != NULL)
	{
		if (data->mapBase != NULL)
			munmap(data->mapBase, data->mapLength);
		if (data->surface != NULL)
		{
			if (data->shmfd < 0)
				IOConnectCallMethod(g_surfaceService, kIOSurfaceMethodRelease, NULL, 0, &data->surface->surfaceID, 1, NULL, 0, NULL, 0);
			free(data->surface);
		}
		if (data->shmfd >= 0)
			close(data->shmfd);

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
	return ((ImplData*) _impl)->surface->planeCount;
}

- (NSInteger)widthOfPlaneAtIndex:(NSUInteger)planeIndex
{
	if (planeIndex >= [self planeCount])
		return 0;

	ImplData* data = (ImplData*) _impl;
	return data->surface->planes[planeIndex].width;
}

- (NSInteger)heightOfPlaneAtIndex:(NSUInteger)planeIndex
{
	if (planeIndex >= [self planeCount])
		return 0;

	ImplData* data = (ImplData*) _impl;
	return data->surface->planes[planeIndex].height;
}

- (NSInteger)bytesPerRowOfPlaneAtIndex:(NSUInteger)planeIndex
{
	if (planeIndex >= [self planeCount])
		return 0;

	ImplData* data = (ImplData*) _impl;
	return data->surface->planes[planeIndex].bytesPerRow;
}

- (NSInteger)bytesPerElementOfPlaneAtIndex:(NSUInteger)planeIndex
{
	if (planeIndex >= [self planeCount])
		return 0;

	ImplData* data = (ImplData*) _impl;
	return data->surface->planes[planeIndex].bytesPerElement;
}

- (NSInteger)elementWidthOfPlaneAtIndex:(NSUInteger)planeIndex
{
	return 1;
}

- (NSInteger)elementHeightOfPlaneAtIndex:(NSUInteger)planeIndex
{
	return 1;
}

- (void *)baseAddressOfPlaneAtIndex:(NSUInteger)planeIndex
{
	if (planeIndex >= [self planeCount])
		return NULL;

	ImplData* data = (ImplData*) _impl;
	return (void*)(data->surface->address + data->surface->planes[planeIndex].memoryOffset);
}

- (void)incrementUseCount
{
	ImplData* data = (ImplData*) _impl;
	if ((data->localUseCount++) == 0)
	{
		uint64_t scalar = data->surface->surfaceID;
		IOConnectCallMethod(g_surfaceService, kIOSurfaceMethodIncrementUseCount, &scalar, 1, NULL, 0, NULL, 0, NULL, 0);
	}
}

- (void)decrementUseCount
{
	ImplData* data = (ImplData*) _impl;
	if (--data->localUseCount == 0)
	{
		uint64_t scalar = data->surface->surfaceID;
		IOConnectCallMethod(g_surfaceService, kIOSurfaceMethodDecrementUseCount, &scalar, 1, NULL, 0, NULL, 0, NULL, 0);
	}
}

- (int32_t) localUseCount
{
	ImplData* data = (ImplData*) _impl;
	return data->localUseCount;
}

- (kern_return_t)lockWithOptions:(IOSurfaceLockOptions)options seed:(nullable uint32_t *)seed
{
	ImplData* data = (ImplData*) _impl;
	if (data->shmfd >= 0)
	{
		if (seed != NULL)
			*seed = 0;
		return kIOSurfaceSuccess;
	}
	struct _IOSurfaceLockUnlock args = {
		.surfaceID = data->surface->surfaceID,
		.options = options,
	};
	return IOConnectCallMethod(g_surfaceService, kIOSurfaceMethodLock, NULL, 0, &args, sizeof(args), NULL, 0, NULL, 0);
}

- (kern_return_t)unlockWithOptions:(IOSurfaceLockOptions)options seed:(nullable uint32_t *)seed
{
	ImplData* data = (ImplData*) _impl;
	if (data->shmfd >= 0)
	{
		if (seed != NULL)
			*seed = 1;
		return kIOSurfaceSuccess;
	}
	struct _IOSurfaceLockUnlock args = {
		.surfaceID = data->surface->surfaceID,
		.options = options,
	};
	return IOConnectCallMethod(g_surfaceService, kIOSurfaceMethodUnlock, NULL, 0, &args, sizeof(args), NULL, 0, NULL, 0);
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector {
    return [NSMethodSignature signatureWithObjCTypes: "v@:"];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation {
    NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), [self class]);
}

@end
