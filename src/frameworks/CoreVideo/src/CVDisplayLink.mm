#include <CoreVideo/CVDisplayLink.h>
#include <mach/mach_time.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSString.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

__attribute__((used)) const char cv_displaylink_x11_v1[] = "cv_displaylink_x11_v1";

#define CV_LINK_HZ 60
static NSString *const kDisplayIDKey = @"CGDirectDisplay";
static NSString *const kCallbackKey = @"callback";
static NSString *const kUserInfoKey = @"userInfo";
static NSString *const kRunningKey = @"running";
static NSString *const kStopKey = @"stop";
static NSString *const kVideoTimeKey = @"videoTime";
static NSString *const kThreadKey = @"thread";

static CGDirectDisplayID cv_x11_display(CGDirectDisplayID displayID)
{
	if (displayID == kCGNullDirectDisplay)
		return 1;
	return displayID;
}

static void cv_fill_timestamp(NSMutableDictionary *self, CVTimeStamp *ts)
{
	int64_t videoTime = [[self objectForKey: kVideoTimeKey] longLongValue];
	memset(ts, 0, sizeof(*ts));
	ts->version = 0;
	ts->videoTimeScale = CV_LINK_HZ;
	ts->videoTime = videoTime;
	ts->hostTime = mach_absolute_time();
	ts->rateScalar = 1.0;
	ts->videoRefreshPeriod = 1;
	ts->flags = kCVTimeStampVideoTimeValid | kCVTimeStampHostTimeValid |
		    kCVTimeStampVideoRefreshPeriodValid |
		    kCVTimeStampRateScalarValid;
}

static NSMutableDictionary *cv_link_new(CGDirectDisplayID displayID)
{
	NSMutableDictionary *self = [[NSMutableDictionary alloc] init];
	[self setObject: @[[NSNumber numberWithUnsignedInt: cv_x11_display(displayID)]]
		 forKey: kDisplayIDKey];
	[self setObject: @0 forKey: kVideoTimeKey];
	[self setObject: @NO forKey: kRunningKey];
	[self setObject: @NO forKey: kStopKey];
	return self;
}

static CVReturn cv_link_out(NSMutableDictionary *self, CVDisplayLinkRef *displayLinkOut)
{
	if (!displayLinkOut) {
		[self release];
		return kCVReturnInvalidArgument;
	}
	if (!self)
		return kCVReturnAllocationFailed;
	*displayLinkOut = (CVDisplayLinkRef) self;
	fprintf(stderr, "cv_displaylink_x11_v1 create display=%u link=%p ret=0\n",
		(unsigned) cv_x11_display(0), (void *) self);
	NSArray *ids = [self objectForKey: kDisplayIDKey];
	fprintf(stderr, "cv_displaylink_x11_v1 create stored=%u ret=0\n",
		(unsigned) [[ids firstObject] unsignedIntValue]);
	fflush(stderr);
	return kCVReturnSuccess;
}

CVReturn CVDisplayLinkCreateWithActiveCGDisplays(CVDisplayLinkRef *displayLinkOut)
{
	return cv_link_out(cv_link_new(1), displayLinkOut);
}

CVReturn CVDisplayLinkCreateWithCGDisplays(CGDirectDisplayID *displayArray, CFIndex count,
					   CVDisplayLinkRef *displayLinkOut)
{
	CGDirectDisplayID id = 1;
	if (displayArray && count > 0)
		id = displayArray[0];
	return cv_link_out(cv_link_new(id), displayLinkOut);
}

CVReturn CVDisplayLinkCreateWithOpenGLDisplayMask(CGOpenGLDisplayMask mask,
						  CVDisplayLinkRef *displayLinkOut)
{
	(void) mask;
	return cv_link_out(cv_link_new(1), displayLinkOut);
}

CVReturn CVDisplayLinkCreateWithCGDisplay(CGDirectDisplayID displayID,
					  CVDisplayLinkRef *displayLinkOut)
{
	return cv_link_out(cv_link_new(displayID), displayLinkOut);
}

static void *cv_link_thread(void *arg)
{
	NSMutableDictionary *self = (NSMutableDictionary *) arg;
	const long nsec = 1000000000L / CV_LINK_HZ;
	while (![[self objectForKey: kStopKey] boolValue]) {
		CVDisplayLinkOutputCallback cb = NULL;
		void *info = NULL;
		NSValue *cbv = [self objectForKey: kCallbackKey];
		if (cbv)
			cb = (CVDisplayLinkOutputCallback) [cbv pointerValue];
		NSValue *uiv = [self objectForKey: kUserInfoKey];
		if (uiv)
			info = [uiv pointerValue];
		CVTimeStamp now, out;
		cv_fill_timestamp(self, &now);
		int64_t vt = [[self objectForKey: kVideoTimeKey] longLongValue] + 1;
		[self setObject: @(vt) forKey: kVideoTimeKey];
		out = now;
		out.videoTime = vt;
		if (cb) {
			CVOptionFlags flagsOut = 0;
			cb((CVDisplayLinkRef) self, &now, &out, 0, &flagsOut, info);
		}
		struct timespec slp = {0, nsec};
		nanosleep(&slp, NULL);
	}
	return NULL;
}

CVReturn CVDisplayLinkStart(CVDisplayLinkRef displayLink)
{
	NSMutableDictionary *self = (NSMutableDictionary *) displayLink;
	if (!self)
		return kCVReturnInvalidArgument;
	if ([[self objectForKey: kRunningKey] boolValue])
		return kCVReturnDisplayLinkAlreadyRunning;
	[self setObject: @NO forKey: kStopKey];
	[self setObject: @YES forKey: kRunningKey];
	pthread_t th;
	if (pthread_create(&th, NULL, cv_link_thread, self) != 0) {
		[self setObject: @NO forKey: kRunningKey];
		return kCVReturnError;
	}
	[self setObject: [NSValue valueWithPointer: (void *) th] forKey: kThreadKey];
	fprintf(stderr, "cv_displaylink_x11_v1 start link=%p\n", (void *) self);
	fflush(stderr);
	return kCVReturnSuccess;
}

CVReturn CVDisplayLinkStop(CVDisplayLinkRef displayLink)
{
	NSMutableDictionary *self = (NSMutableDictionary *) displayLink;
	if (!self)
		return kCVReturnInvalidArgument;
	if (![[self objectForKey: kRunningKey] boolValue])
		return kCVReturnDisplayLinkNotRunning;
	[self setObject: @YES forKey: kStopKey];
	NSValue *thv = [self objectForKey: kThreadKey];
	if (thv) {
		pthread_t th = (pthread_t) [thv pointerValue];
		pthread_join(th, NULL);
	}
	[self setObject: @NO forKey: kRunningKey];
	return kCVReturnSuccess;
}

Boolean CVDisplayLinkIsRunning(CVDisplayLinkRef displayLink)
{
	NSMutableDictionary *self = (NSMutableDictionary *) displayLink;
	if (!self)
		return false;
	return [[self objectForKey: kRunningKey] boolValue];
}

void CVDisplayLinkRelease(CVDisplayLinkRef displayLink)
{
	NSMutableDictionary *self = (NSMutableDictionary *) displayLink;
	if (!self)
		return;
	if ([[self objectForKey: kRunningKey] boolValue])
		CVDisplayLinkStop(displayLink);
	[self release];
}

CVDisplayLinkRef CVDisplayLinkRetain(CVDisplayLinkRef displayLink)
{
	NSMutableDictionary *self = (NSMutableDictionary *) displayLink;
	return (CVDisplayLinkRef) [self retain];
}

CVReturn CVDisplayLinkSetOutputCallback(CVDisplayLinkRef displayLink,
					CVDisplayLinkOutputCallback callback, void *userInfo)
{
	NSMutableDictionary *self = (NSMutableDictionary *) displayLink;
	if (!self)
		return kCVReturnInvalidArgument;
	if (callback)
		[self setObject: [NSValue valueWithPointer: (void *) callback] forKey: kCallbackKey];
	else
		[self removeObjectForKey: kCallbackKey];
	[self setObject: [NSValue valueWithPointer: userInfo] forKey: kUserInfoKey];
	return kCVReturnSuccess;
}

CVReturn CVDisplayLinkSetCurrentCGDisplay(CVDisplayLinkRef displayLink, CGDirectDisplayID displayID)
{
	NSMutableDictionary *self = (NSMutableDictionary *) displayLink;
	if (!self)
		return kCVReturnInvalidArgument;
	[self setObject: @[[NSNumber numberWithUnsignedInt: cv_x11_display(displayID)]]
		 forKey: kDisplayIDKey];
	return kCVReturnSuccess;
}

CVReturn CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(CVDisplayLinkRef displayLink,
							   CGLContextObj cglContext,
							   CGLPixelFormatObj cglPixelFormat)
{
	(void) cglContext;
	(void) cglPixelFormat;
	return CVDisplayLinkSetCurrentCGDisplay(displayLink, 1);
}

CGDirectDisplayID CVDisplayLinkGetCurrentCGDisplay(CVDisplayLinkRef displayLink)
{
	NSMutableDictionary *self = (NSMutableDictionary *) displayLink;
	if (!self)
		return 1;
	NSArray *ids = [self objectForKey: kDisplayIDKey];
	if ([ids count] > 0) {
		CGDirectDisplayID id = (CGDirectDisplayID) [[ids firstObject] unsignedIntValue];
		if (id != kCGNullDirectDisplay)
			return id;
	}
	return 1;
}

CVTime CVDisplayLinkGetNominalOutputVideoRefreshPeriod(CVDisplayLinkRef displayLink)
{
	CVTime time;
	(void) displayLink;
	time.timeValue = 1;
	time.timeScale = CV_LINK_HZ;
	time.flags = 0;
	return time;
}

CVTime CVDisplayLinkGetOutputVideoLatency(CVDisplayLinkRef displayLink)
{
	CVTime time;
	(void) displayLink;
	time.timeValue = 0;
	time.timeScale = CV_LINK_HZ;
	time.flags = 0;
	return time;
}

double CVDisplayLinkGetActualOutputVideoRefreshPeriod(CVDisplayLinkRef displayLink)
{
	(void) displayLink;
	return 1.0 / (double) CV_LINK_HZ;
}

CVReturn CVDisplayLinkGetCurrentTime(CVDisplayLinkRef displayLink, CVTimeStamp *outTime)
{
	NSMutableDictionary *self = (NSMutableDictionary *) displayLink;
	if (!self || !outTime)
		return kCVReturnInvalidArgument;
	cv_fill_timestamp(self, outTime);
	return kCVReturnSuccess;
}

CVReturn CVDisplayLinkTranslateTime(CVDisplayLinkRef displayLink, const CVTimeStamp *inTime,
				    CVTimeStamp *outTime)
{
	if (!displayLink || !inTime || !outTime)
		return kCVReturnInvalidArgument;
	*outTime = *inTime;
	return kCVReturnSuccess;
}

CFTypeID CVDisplayLinkGetTypeID(void)
{
	static CFTypeID id;
	if (!id) {
		NSMutableDictionary *d = [NSMutableDictionary dictionary];
		id = CFGetTypeID((CFTypeRef) d);
	}
	return id;
}
