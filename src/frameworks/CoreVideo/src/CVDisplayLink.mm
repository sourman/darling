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
#include <CoreFoundation/CFRunLoop.h>

static const NSString* kDirectDisplayArray = @"CGDirectDisplay";
static NSString* const kOutputCallback = @"CVDisplayLinkOutputCallback";
static NSString* const kOutputUserInfo = @"CVDisplayLinkOutputUserInfo";
static NSString* const kRunning = @"CVDisplayLinkRunning";
static NSString* const kStarterRunLoop = @"CVDisplayLinkStarterRunLoop";
static NSString* const kStarterSource = @"CVDisplayLinkStarterSource";

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
	if (logged < 4) {
		uint32_t display_id = 0;
		uint64_t registered = 0;
		uint64_t fire_id = 0;
		logged++;
		uintptr_t task_runner = 0;
		uintptr_t sequence = 0;
		if (userInfo != NULL) {
			display_id = *(uint32_t *)((char *)userInfo + 0x0c);
			registered = *(uint64_t *)((char *)userInfo + 0x10);
			task_runner = *(uintptr_t *)((char *)userInfo + 0x48);
			if (task_runner > 0x1000)
				sequence = *(uintptr_t *)(task_runner + 0x10);
		}
		pthread_threadid_np(NULL, &fire_id);
		fprintf(stderr, "CVDisplayLink fire callback %p scale %d period %lld host %llu flags %llx display %u registered %llu fire %llu runner %lx sequence %lx\n",
				callback, now.videoTimeScale,
				(long long) now.videoRefreshPeriod,
				(unsigned long long) now.hostTime,
				(unsigned long long) now.flags,
				display_id, (unsigned long long) registered,
				(unsigned long long) fire_id,
				(unsigned long) task_runner,
				(unsigned long) sequence);
		fflush(stderr);
	}
	callback((CVDisplayLinkRef) self, &now, &output, 0, &flagsOut, userInfo);
	static int ran_logs;
	if (ran_logs < 8 && userInfo != NULL) {
		ran_logs++;
		uintptr_t task_runner = *(uintptr_t *)((char *)userInfo + 0x48);
		uintptr_t sequence = 0;
		uintptr_t queue = 0;
		uintptr_t bound = 0;
		if (task_runner > 0x1000)
			sequence = *(uintptr_t *)(task_runner + 0x10);
		if (sequence > 0x1000)
			queue = *(uintptr_t *)(sequence + 0x38);
		if (queue > 0x1000) {
			uintptr_t assoc = *(uintptr_t *)(queue + 0x18);
			if (assoc > 0x1000)
				bound = *(uintptr_t *)(assoc + 0x8);
		}
		uintptr_t head = 0;
		uintptr_t tail = 0;
		if (queue > 0x1000) {
			head = *(uintptr_t *)(queue + 0x88);
			tail = *(uintptr_t *)(queue + 0x90);
		}
		/*
		 * The display-link callback posts OnDisplayLinkCallback onto this
		 * queue. head is the RunWork cursor at +0x88 and tail is the
		 * producer cursor at +0x90. A tail that moves and a head that
		 * later catches it means that callback ran.
		 */
		fprintf(stderr, "CVDisplayLink ran self=%lx bound=%lx queue=%lx head=%lu tail=%lu\n",
			(unsigned long)pthread_self(), (unsigned long)bound,
			(unsigned long)queue,
			(unsigned long)head, (unsigned long)tail);
		fflush(stderr);
	}
}

static void displayLinkPerform(void *info)
{
	fireDisplayLink((NSMutableDictionary *)info);
}

static void* displayLinkThread(void* arg)
{
	NSMutableDictionary* self = (NSMutableDictionary*) arg;
	CFRunLoopRef rl = (CFRunLoopRef)[[self objectForKey: kStarterRunLoop] unsignedLongValue];
	CFRunLoopSourceRef source = (CFRunLoopSourceRef)[[self objectForKey: kStarterSource] unsignedLongValue];
	while ([[self objectForKey: kRunning] boolValue]) {
		if (source != NULL && rl != NULL) {
			CFRunLoopSourceSignal(source);
			CFRunLoopWakeUp(rl);
		}
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
	CFRunLoopRef rl = CFRunLoopGetCurrent();
	CFRetain(rl);
	CFRunLoopSourceContext ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.info = self;
	ctx.perform = displayLinkPerform;
	CFRunLoopSourceRef source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &ctx);
	CFRunLoopAddSource(rl, source, kCFRunLoopCommonModes);
	[self setObject: [NSNumber numberWithUnsignedLong: (unsigned long)rl]
			forKey: kStarterRunLoop];
	[self setObject: [NSNumber numberWithUnsignedLong: (unsigned long)source]
			forKey: kStarterSource];
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
