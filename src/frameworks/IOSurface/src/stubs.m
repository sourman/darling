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

#include <stdlib.h>
#include <stdio.h>
#import <IOSurface/IOSurface.h>
#import <IOSurface/IOSurfaceObjC.h>

#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>

static int verbose = 0;

__attribute__((constructor))
static void initme(void) {
    verbose = getenv("STUB_VERBOSE") != NULL;
}


IOSurfaceRef _Nullable IOSurfaceLookup(IOSurfaceID csid){
    return (IOSurfaceRef)[[IOSurface alloc] initWithMachPort: (mach_port_t)csid];
}


IOSurfaceID IOSurfaceGetID(IOSurfaceRef buffer){
    return (IOSurfaceID)[(IOSurface*)buffer hash];
}


size_t IOSurfaceGetAllocSize(IOSurfaceRef buffer){
    size_t rows = [(IOSurface*)buffer bytesPerRow];
    size_t height = [(IOSurface*)buffer height];
    return rows * height;
}



size_t IOSurfaceGetNumberOfComponentsOfPlane(IOSurfaceRef buffer, size_t planeIndex){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}


IOSurfaceComponentName IOSurfaceGetNameOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}


IOSurfaceComponentType IOSurfaceGetTypeOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}


IOSurfaceComponentRange IOSurfaceGetRangeOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}


size_t IOSurfaceGetBitDepthOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}


size_t IOSurfaceGetBitOffsetOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}


IOSurfaceSubsampling IOSurfaceGetSubsampling(IOSurfaceRef buffer){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}



mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef buffer){
    mach_port_t port = MACH_PORT_NULL;
    IOSurfaceID surfaceID = IOSurfaceGetID(buffer);
    kern_return_t kr;

    if (surfaceID == 0)
        return MACH_PORT_NULL;
    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    if (kr != KERN_SUCCESS)
        return MACH_PORT_NULL;
    kr = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS)
        return MACH_PORT_NULL;
    /* The context stays on the port object when the send right moves. */
    mach_port_set_context(mach_task_self(), port, (mach_port_context_t)surfaceID);
    return port;
}


size_t IOSurfaceGetPropertyMaximum(CFStringRef property){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}

size_t IOSurfaceGetPropertyAlignment(CFStringRef property){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}



size_t IOSurfaceAlignProperty(CFStringRef property, size_t value){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}



Boolean IOSurfaceAllowsPixelSizeCasting(IOSurfaceRef buffer){
    if (verbose) printf("STUB: %s called\n", __FUNCTION__);
    return 0;
}


