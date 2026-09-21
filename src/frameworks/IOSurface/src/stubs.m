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
#include <CoreFoundation/CoreFoundation.h>

#define kPixelBGRA 'BGRA'
#define kPixelRGBA 'RGBA'
#define kPixelBGRX 'BGRX'
#define kPixelRGBX 'RGBX'

static int is_bgra_family(OSType fmt)
{
	return fmt == kPixelBGRA || fmt == kPixelRGBA || fmt == kPixelBGRX ||
	       fmt == kPixelRGBX || fmt == 0;
}

size_t IOSurfaceGetNumberOfComponentsOfPlane(IOSurfaceRef buffer, size_t planeIndex)
{
	if (!buffer)
		return 0;
	if (planeIndex > 0 && IOSurfaceGetPlaneCount(buffer) == 0 && planeIndex != 0)
		return 0;
	if (planeIndex > 0 && IOSurfaceGetPlaneCount(buffer) != 0 &&
	    planeIndex >= IOSurfaceGetPlaneCount(buffer))
		return 0;
	if (is_bgra_family(IOSurfaceGetPixelFormat(buffer)))
		return 4;
	size_t bpe = IOSurfaceGetBytesPerElementOfPlane(buffer, planeIndex);
	return bpe ? bpe : 4;
}

IOSurfaceComponentName IOSurfaceGetNameOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex)
{
	(void) planeIndex;
	if (!is_bgra_family(IOSurfaceGetPixelFormat(buffer)))
		return kIOSurfaceComponentNameUnknown;
	/* BGRA memory order */
	switch (componentIndex) {
	case 0: return kIOSurfaceComponentNameBlue;
	case 1: return kIOSurfaceComponentNameGreen;
	case 2: return kIOSurfaceComponentNameRed;
	case 3: return kIOSurfaceComponentNameAlpha;
	default: return kIOSurfaceComponentNameUnknown;
	}
}

IOSurfaceComponentType IOSurfaceGetTypeOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex)
{
	(void) buffer; (void) planeIndex; (void) componentIndex;
	return kIOSurfaceComponentTypeUnsignedInteger;
}

IOSurfaceComponentRange IOSurfaceGetRangeOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex)
{
	(void) buffer; (void) planeIndex; (void) componentIndex;
	return kIOSurfaceComponentRangeFullRange;
}

size_t IOSurfaceGetBitDepthOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex)
{
	(void) planeIndex; (void) componentIndex;
	if (!buffer)
		return 0;
	size_t bpe = IOSurfaceGetBytesPerElementOfPlane(buffer, 0);
	size_t n = IOSurfaceGetNumberOfComponentsOfPlane(buffer, 0);
	if (n == 0)
		return 8;
	return (bpe * 8) / n;
}

size_t IOSurfaceGetBitOffsetOfComponentOfPlane(IOSurfaceRef buffer, size_t planeIndex, size_t componentIndex)
{
	(void) planeIndex;
	return componentIndex * IOSurfaceGetBitDepthOfComponentOfPlane(buffer, 0, componentIndex);
}

IOSurfaceSubsampling IOSurfaceGetSubsampling(IOSurfaceRef buffer)
{
	(void) buffer;
	return kIOSurfaceSubsamplingNone;
}
