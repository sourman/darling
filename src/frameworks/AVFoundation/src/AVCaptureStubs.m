#import <Foundation/Foundation.h>

@interface AVCaptureDeviceDiscoverySession : NSObject
@end
@implementation AVCaptureDeviceDiscoverySession
- (NSArray *)devices
{
	return @[];
}
- (NSArray *)supportedMultiCamDeviceSets
{
	return @[];
}
+ (instancetype)discoverySessionWithDeviceTypes:(NSArray *)deviceTypes
                                      mediaType:(NSString *)mediaType
                                       position:(NSInteger)position
{
	(void)deviceTypes;
	(void)mediaType;
	(void)position;
	return [[self alloc] init];
}
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
	(void)aSelector;
	return [NSMethodSignature signatureWithObjCTypes: "@@:"];
}
- (void)forwardInvocation:(NSInvocation *)anInvocation
{
	NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), [self class]);
}
@end

@interface AVCapturePhotoOutput : NSObject
@end
@implementation AVCapturePhotoOutput
@end

@interface AVCapturePhotoSettings : NSObject
@end
@implementation AVCapturePhotoSettings
@end
