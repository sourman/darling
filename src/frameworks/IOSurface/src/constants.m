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
#import <IOSurface/IOSurfaceRef.h>
#import <IOSurface/IOSurfaceObjC.h>

const CFStringRef kIOSurfaceAllocSize = CFSTR("IOSurfaceAllocSize");
const CFStringRef kIOSurfaceWidth = CFSTR("IOSurfaceWidth");
const CFStringRef kIOSurfaceHeight = CFSTR("IOSurfaceHeight");
const CFStringRef kIOSurfaceBytesPerRow = CFSTR("IOSurfaceBytesPerRow");
const CFStringRef kIOSurfaceBytesPerElement = CFSTR("IOSurfaceBytesPerElement");
const CFStringRef kIOSurfaceElementWidth = CFSTR("IOSurfaceElementWidth");
const CFStringRef kIOSurfaceElementHeight = CFSTR("IOSurfaceElementHeight");
const CFStringRef kIOSurfaceOffset = CFSTR("IOSurfaceOffset");
const CFStringRef kIOSurfacePlaneInfo = CFSTR("IOSurfacePlaneInfo");
const CFStringRef kIOSurfacePlaneWidth = CFSTR("IOSurfacePlaneWidth");
const CFStringRef kIOSurfacePlaneHeight = CFSTR("IOSurfacePlaneHeight");
const CFStringRef kIOSurfacePlaneBytesPerRow = CFSTR("IOSurfacePlaneBytesPerRow");
const CFStringRef kIOSurfacePlaneOffset = CFSTR("IOSurfacePlaneOffset");
const CFStringRef kIOSurfacePlaneSize = CFSTR("IOSurfacePlaneSize");
const CFStringRef kIOSurfacePlaneBase = CFSTR("IOSurfacePlaneBase");
const CFStringRef kIOSurfacePlaneBitsPerElement = CFSTR("IOSurfacePlaneBitsPerElement");
const CFStringRef kIOSurfacePlaneBytesPerElement = CFSTR("IOSurfacePlaneBytesPerElement");
const CFStringRef kIOSurfacePlaneElementWidth = CFSTR("IOSurfacePlaneElementWidth");
const CFStringRef kIOSurfacePlaneElementHeight = CFSTR("IOSurfacePlaneElementHeight");
const CFStringRef kIOSurfaceCacheMode = CFSTR("IOSurfaceCacheMode");
const CFStringRef kIOSurfaceIsGlobal = CFSTR("IOSurfaceIsGlobal");
const CFStringRef kIOSurfacePixelFormat = CFSTR("IOSurfacePixelFormat");
const CFStringRef kIOSurfacePixelSizeCastingAllowed = CFSTR("IOSurfacePixelSizeCastingAllowed");
const CFStringRef kIOSurfacePlaneComponentBitDepths = CFSTR("IOSurfacePlaneComponentBitDepths");
const CFStringRef kIOSurfacePlaneComponentBitOffsets = CFSTR("IOSurfacePlaneComponentBitOffsets");
const CFStringRef kIOSurfaceColorSpace = CFSTR("IOSurfaceColorSpace");
const CFStringRef kIOSurfaceIsDisplayable = CFSTR("IsDisplayable");
const CFStringRef kIOSurfaceName = CFSTR("IOSurfaceName");
const CFStringRef kIOSurfacePixelCastingAllowed = CFSTR("IOSurfacePixelSizeCastingAllowed");
const CFStringRef kIOSurfaceProtectionOptions = CFSTR("IOSurfaceProtectionOptions");
const CFStringRef kIOSurfaceYCbCrMatrix = CFSTR("IOSurfaceYCbCrMatrix");
const CFStringRef kIOSurfaceYCbCrMatrix_ITU_R_601_4_String = CFSTR("ITU_R_601_4");
const CFStringRef kIOSurfaceYCbCrMatrix_ITU_R_709_2_String = CFSTR("ITU_R_709_2");

const CFStringRef kIOSurfacePlaneComponentNames = CFSTR("IOSurfacePlaneComponentNames");
const CFStringRef kIOSurfacePlaneComponentTypes = CFSTR("IOSurfacePlaneComponentTypes");
const CFStringRef kIOSurfacePlaneComponentRanges = CFSTR("IOSurfacePlaneComponentRanges");
const CFStringRef kIOSurfaceSubsampling = CFSTR("IOSurfaceSubsampling");

/* Real macOS keys are IOSurfaceWidth etc., not the ObjC enum identifier. */
IOSurfacePropertyKey IOSurfacePropertyKeyAllocSize = @"IOSurfaceAllocSize";
IOSurfacePropertyKey IOSurfacePropertyKeyWidth = @"IOSurfaceWidth";
IOSurfacePropertyKey IOSurfacePropertyKeyHeight = @"IOSurfaceHeight";
IOSurfacePropertyKey IOSurfacePropertyKeyBytesPerRow = @"IOSurfaceBytesPerRow";
IOSurfacePropertyKey IOSurfacePropertyKeyBytesPerElement = @"IOSurfaceBytesPerElement";
IOSurfacePropertyKey IOSurfacePropertyKeyElementWidth = @"IOSurfaceElementWidth";
IOSurfacePropertyKey IOSurfacePropertyKeyElementHeight = @"IOSurfaceElementHeight";
IOSurfacePropertyKey IOSurfacePropertyKeyOffset = @"IOSurfaceOffset";
IOSurfacePropertyKey IOSurfacePropertyKeyPlaneInfo = @"IOSurfacePlaneInfo";
IOSurfacePropertyKey IOSurfacePropertyKeyPlaneWidth = @"IOSurfacePlaneWidth";
IOSurfacePropertyKey IOSurfacePropertyKeyPlaneHeight = @"IOSurfacePlaneHeight";
IOSurfacePropertyKey IOSurfacePropertyKeyPlaneBytesPerRow = @"IOSurfacePlaneBytesPerRow";
IOSurfacePropertyKey IOSurfacePropertyKeyPlaneOffset = @"IOSurfacePlaneOffset";
IOSurfacePropertyKey IOSurfacePropertyKeyPlaneSize = @"IOSurfacePlaneSize";
IOSurfacePropertyKey IOSurfacePropertyKeyPlaneBase = @"IOSurfacePlaneBase";
IOSurfacePropertyKey IOSurfacePropertyKeyPlaneBytesPerElement = @"IOSurfacePlaneBytesPerElement";
IOSurfacePropertyKey IOSurfacePropertyKeyPlaneElementWidth = @"IOSurfacePlaneElementWidth";
IOSurfacePropertyKey IOSurfacePropertyKeyPlaneElementHeight = @"IOSurfacePlaneElementHeight";
IOSurfacePropertyKey IOSurfacePropertyKeyCacheMode = @"IOSurfaceCacheMode";
IOSurfacePropertyKey IOSurfacePropertyKeyPixelFormat = @"IOSurfacePixelFormat";
IOSurfacePropertyKey IOSurfacePropertyKeyPixelSizeCastingAllowed = @"IOSurfacePixelSizeCastingAllowed";
IOSurfacePropertyKey IOSurfacePropertyAllocSizeKey = @"IOSurfaceAllocSize";

