#include <CoreVideo/CVDisplayLink.h>
#include <memory>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <mach/mach_time.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSScreen.h>
#import <Foundation/NSDictionary.h>
#include <CoreGraphics/CGWindow.h>
#include <CoreGraphics/CGDirectDisplay.h>

static const NSString* kDirectDisplayArray = @"CGDirectDisplay";
static NSString* const kOutputCallback = @"CVDisplayLinkOutputCallback";
static NSString* const kOutputUserInfo = @"CVDisplayLinkOutputUserInfo";
static NSString* const kRunning = @"CVDisplayLinkRunning";

static void fillTimeStamp(CVTimeStamp* ts)
{
	/*
	 * Darwin CLOCK_MONOTONIC is 6. Passed to Linux that is
	 * CLOCK_MONOTONIC_COARSE, a different clock from mach_absolute_time
	 * (CLOCK_MONOTONIC, id 1). Chromium turns hostTime into TimeTicks with
	 * FromMachAbsoluteTime, so hostTime has to be that same value.
	 */
	uint64_t host = mach_absolute_time();
	memset(ts, 0, sizeof(*ts));
	ts->version = 0;
	ts->videoTimeScale = 60000;
	ts->videoTime = (int64_t) (host / (1000000000ull / 60000ull));
	ts->hostTime = host;
	ts->rateScalar = 1.0;
	ts->videoRefreshPeriod = 1000;
	ts->flags = kCVTimeStampVideoTimeValid | kCVTimeStampHostTimeValid |
			kCVTimeStampRateScalarValid | kCVTimeStampVideoRefreshPeriodValid;
}

static void fireDisplayLink(NSMutableDictionary* self)
{
	NSNumber* callbackNumber = [self objectForKey: kOutputCallback];
	if (callbackNumber == nil) {
		fprintf(stderr, "CVDisplayLink fire skipped: no callback\n");
		fflush(stderr);
		return;
	}
	CVDisplayLinkOutputCallback callback =
			(CVDisplayLinkOutputCallback) [callbackNumber unsignedLongValue];
	if (callback == NULL) {
		fprintf(stderr, "CVDisplayLink fire skipped: null callback\n");
		fflush(stderr);
		return;
	}
	void* userInfo = (void*) [[self objectForKey: kOutputUserInfo] unsignedLongValue];
	CVTimeStamp now;
	CVTimeStamp output;
	fillTimeStamp(&now);
	output = now;
	CVOptionFlags flagsOut = 0;
	/*
	 * DisplayLinkCallback's first skip is ComputeVSyncParameters:
	 * videoRefreshPeriod (numerator) or videoTimeScale (denominator)
	 * of 0 returns before a BeginFrame is posted. Both have to be
	 * nonzero, and hostTime has to be a mach absolute time.
	 */
	if (now.videoTimeScale == 0)
		now.videoTimeScale = 60000;
	if (now.videoRefreshPeriod == 0)
		now.videoRefreshPeriod = now.videoTimeScale / 60;
	if (now.hostTime == 0)
		now.hostTime = mach_absolute_time();
	if (now.hostTime == 0)
		now.hostTime = 1;
	now.flags |= kCVTimeStampVideoTimeValid | kCVTimeStampHostTimeValid |
			kCVTimeStampRateScalarValid | kCVTimeStampVideoRefreshPeriodValid;
	output = now;
	static int logged;
	if (logged < 2) {
		logged++;
		fprintf(stderr, "CVDisplayLink fire callback %p scale %d period %lld host %llu flags %llx\n",
				callback, now.videoTimeScale,
				(long long) now.videoRefreshPeriod,
				(unsigned long long) now.hostTime,
				(unsigned long long) now.flags);
		fflush(stderr);
	}
	callback((CVDisplayLinkRef) self, &now, &output, 0, &flagsOut, userInfo);
}

static void* displayLinkThread(void* arg)
{
	NSMutableDictionary* self = (NSMutableDictionary*) arg;
	while ([[self objectForKey: kRunning] boolValue]) {
		fireDisplayLink(self);
		usleep(16667);
	}
	[self release];
	return NULL;
}

CVReturn CVDisplayLinkCreateWithActiveCGDisplays(CVDisplayLinkRef* displayLinkOut)
{
	uint32_t displayCount;
	std::unique_ptr<CGDirectDisplayID[]> displays;

	CGError err = CGGetActiveDisplayList(0, nullptr, &displayCount);
	if (err != kCGErrorSuccess)
		return err;

	displays.reset(new CGDirectDisplayID[displayCount]);

	err = CGGetActiveDisplayList(displayCount, displays.get(), &displayCount);
	if (err != kCGErrorSuccess)
		return err;
	
	NSMutableDictionary* self = [[NSMutableDictionary alloc] init];
	NSMutableArray* array = [NSMutableArray arrayWithCapacity: displayCount];

	for (int i = 0; i < displayCount; i++)
		[array addObject: [NSNumber numberWithInt: displays[i]]];

	[self setObject: array
			forKey: kDirectDisplayArray];

	*displayLinkOut = (CVDisplayLinkRef) self;
	fprintf(stderr, "CVDisplayLinkCreateWithActiveCGDisplays count=%u\n",
			displayCount);
	fflush(stderr);
	return kCVReturnSuccess;
}

CVReturn CVDisplayLinkStart(CVDisplayLinkRef displayLink)
{
	if (!displayLink)
		return kCVReturnInvalidArgument;
	NSMutableDictionary* self = (NSMutableDictionary*) displayLink;
	if ([[self objectForKey: kRunning] boolValue])
		return kCVReturnSuccess;
	[self setObject: @YES forKey: kRunning];
	[self retain];
	pthread_t thread;
	if (pthread_create(&thread, NULL, displayLinkThread, self) != 0) {
		[self setObject: @NO forKey: kRunning];
		[self release];
		return kCVReturnError;
	}
	pthread_detach(thread);
	fprintf(stderr, "CVDisplayLinkStart scheduled\n");
	fflush(stderr);
	return kCVReturnSuccess;
}

CVReturn CVDisplayLinkStop(CVDisplayLinkRef displayLink)
{
	if (!displayLink)
		return kCVReturnInvalidArgument;
	NSMutableDictionary* self = (NSMutableDictionary*) displayLink;
	[self setObject: @NO forKey: kRunning];
	return kCVReturnSuccess;
}

Boolean CVDisplayLinkIsRunning(CVDisplayLinkRef displayLink)
{
	if (!displayLink)
		return false;
	NSMutableDictionary* self = (NSMutableDictionary*) displayLink;
	return [[self objectForKey: kRunning] boolValue];
}

void CVDisplayLinkRelease(CVDisplayLinkRef displayLink)
{
	NSMutableDictionary* self = (NSMutableDictionary*) displayLink;
	[self release];
}

CVReturn CVDisplayLinkSetOutputCallback(CVDisplayLinkRef displayLink, CVDisplayLinkOutputCallback callback, void *userInfo)
{
	if (!displayLink)
		return kCVReturnInvalidArgument;
	NSMutableDictionary* self = (NSMutableDictionary*) displayLink;
	[self setObject: [NSNumber numberWithUnsignedLong: (unsigned long) callback]
			forKey: kOutputCallback];
	[self setObject: [NSNumber numberWithUnsignedLong: (unsigned long) userInfo]
			forKey: kOutputUserInfo];
	fprintf(stderr, "CVDisplayLinkSetOutputCallback %p\n", callback);
	fflush(stderr);
	// Chromium sets the callback and then waits for a vsync. Start the
	// thread here so a missed CVDisplayLinkStart still presents frames.
	if (![[self objectForKey: kRunning] boolValue])
		return CVDisplayLinkStart(displayLink);
	return kCVReturnSuccess;
}

CVDisplayLinkRef CVDisplayLinkRetain(CVDisplayLinkRef displayLink)
{
	return (CVDisplayLinkRef) [(NSMutableDictionary*) displayLink retain];
}

CVReturn CVDisplayLinkGetCurrentTime(CVDisplayLinkRef displayLink, CVTimeStamp *outTime)
{
	if (!displayLink || !outTime)
		return kCVReturnInvalidArgument;
	fillTimeStamp(outTime);
	return kCVReturnSuccess;
}

CVReturn CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(CVDisplayLinkRef displayLink, CGLContextObj cglContext, CGLPixelFormatObj cglPixelFormat)
{
	if (!displayLink)
		return kCVReturnInvalidArgument;

	NSArray *windowArray = [[NSClassFromString(@"NSApplication") sharedApplication] windows];
	if (!windowArray)
		return kCVReturnError;

	for (NSWindow* window in windowArray)
	{
		CGWindow* cgw = [window platformWindow];
		CGLContextObj ctxt = [cgw cglContext];
		if (ctxt == cglContext)
		{
			CGDirectDisplayID displayID = [window.screen cgDirectDisplayID];
			NSMutableDictionary* self = (NSMutableDictionary*) displayLink;

			[self setObject: @[[NSNumber numberWithInt: displayID]]
					forKey: kDirectDisplayArray];
			return kCVReturnSuccess;
		}
	}
	
	return kCVReturnError;
}

CGDirectDisplayID CVDisplayLinkGetCurrentCGDisplay( CVDisplayLinkRef CV_NONNULL displayLink )
{
	if (!displayLink)
		return kCVReturnInvalidArgument;

	NSMutableDictionary* self = (NSMutableDictionary*) displayLink;
	NSArray* ids = self[kDirectDisplayArray];

	if ([ids count] > 0)
		return (CGDirectDisplayID) [[ids firstObject] intValue];
	return kCGNullDirectDisplay;
}

CVReturn CVDisplayLinkCreateWithCGDisplay(
    CGDirectDisplayID displayID,
    CV_RETURNS_RETAINED_PARAMETER CVDisplayLinkRef CV_NULLABLE * CV_NONNULL displayLinkOut )
{
	// Chromium passes 0 here. That is kCGNullDirectDisplay, but it means
	// "the main display" for this call; rejecting it skips the vsync thread.
	if (displayID == kCGNullDirectDisplay)
		displayID = CGMainDisplayID();
	if (displayID == kCGNullDirectDisplay)
		return kCVReturnInvalidArgument;

	NSMutableDictionary* self = [[NSMutableDictionary alloc] init];
	[self setObject: @[ [NSNumber numberWithInt: displayID] ]
			forKey: @"CGDirectDisplay"];

	*displayLinkOut = (CVDisplayLinkRef) self;
	fprintf(stderr, "CVDisplayLinkCreateWithCGDisplay id=%u\n", displayID);
	fflush(stderr);
	return kCVReturnSuccess;
}

CVTime CVDisplayLinkGetNominalOutputVideoRefreshPeriod( CVDisplayLinkRef CV_NONNULL displayLink )
{
	CVTime time = { 0, 0, kCVTimeIsIndefinite };
	CGDirectDisplayID displayId = CVDisplayLinkGetCurrentCGDisplay(displayLink);
	if (displayId == kCGNullDirectDisplay)
		return time;

	CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayId);
	if (!mode)
		return time;

	double rate = CGDisplayModeGetRefreshRate(mode);
	if (rate < 1.0)
		rate = 60.0;

	time.flags = 0;
	time.timeValue = 1.0;
	time.timeScale = rate;

	CGDisplayModeRelease(mode);
	return time;
}
