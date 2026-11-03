#include <CoreVideo/CVDisplayLink.h>
#include <memory>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
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
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	uint64_t host = (uint64_t) now.tv_sec * 1000000000ull + (uint64_t) now.tv_nsec;
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
	if (callbackNumber == nil)
		return;
	CVDisplayLinkOutputCallback callback =
			(CVDisplayLinkOutputCallback) [callbackNumber unsignedLongValue];
	if (callback == NULL)
		return;
	void* userInfo = (void*) [[self objectForKey: kOutputUserInfo] unsignedLongValue];
	CVTimeStamp now;
	CVTimeStamp output;
	fillTimeStamp(&now);
	output = now;
	CVOptionFlags flagsOut = 0;
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
	if (displayID == kCGNullDirectDisplay)
		return kCVReturnInvalidArgument;

	NSMutableDictionary* self = [[NSMutableDictionary alloc] init];
	[self setObject: @[ [NSNumber numberWithInt: displayID] ]
			forKey: @"CGDirectDisplay"];

	*displayLinkOut = (CVDisplayLinkRef) self;
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
