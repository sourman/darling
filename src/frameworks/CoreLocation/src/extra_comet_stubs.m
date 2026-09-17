/* Comet startup cluster: CLLocationManager class + instance APIs. */
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <CoreLocation/CLLocationManager.h>

NSString *kCLLocationAccuracyBest = @"kCLLocationAccuracyBest";
NSString *kCLLocationAccuracyHundredMeters = @"kCLLocationAccuracyHundredMeters";

const char comet_startup_cluster_stubs_v1[] = "comet_startup_cluster_stubs_v1";

enum {
	kCLAuthorizationStatusNotDetermined = 0,
	kCLAuthorizationStatusRestricted = 1,
	kCLAuthorizationStatusDenied = 2,
	kCLAuthorizationStatusAuthorizedAlways = 3,
	kCLAuthorizationStatusAuthorizedWhenInUse = 4
};

@implementation CLLocationManager (CometStartupCluster)

+ (NSInteger)authorizationStatus
{
	return kCLAuthorizationStatusDenied;
}

+ (BOOL)locationServicesEnabled
{
	return NO;
}

+ (BOOL)headingAvailable
{
	return NO;
}

+ (BOOL)significantLocationChangeMonitoringAvailable
{
	return NO;
}

+ (BOOL)isRangingAvailable
{
	return NO;
}

+ (BOOL)isMonitoringAvailableForClass:(Class)regionClass
{
	return NO;
}

+ (BOOL)deferredLocationUpdatesAvailable
{
	return NO;
}

- (id)delegate
{
	return nil;
}

- (void)setDelegate:(id)delegate
{
}

- (NSInteger)authorizationStatus
{
	return kCLAuthorizationStatusDenied;
}

- (NSInteger)accuracyAuthorization
{
	return 0;
}

- (void)requestWhenInUseAuthorization
{
}

- (void)requestAlwaysAuthorization
{
}

- (void)requestTemporaryFullAccuracyAuthorizationWithPurposeKey:(NSString *)purposeKey
{
}

- (void)requestTemporaryFullAccuracyAuthorizationWithPurposeKey:(NSString *)purposeKey
                                                     completion:(void (^)(NSError *error))completion
{
	if (completion)
		completion(nil);
}

- (void)startUpdatingLocation
{
}

- (void)stopUpdatingLocation
{
}

- (void)requestLocation
{
}

- (void)startUpdatingHeading
{
}

- (void)stopUpdatingHeading
{
}

- (id)location
{
	return nil;
}

- (id)heading
{
	return nil;
}

- (void)setDesiredAccuracy:(double)accuracy
{
}

- (double)desiredAccuracy
{
	return -1.0;
}

- (void)setDistanceFilter:(double)filter
{
}

- (double)distanceFilter
{
	return -1.0;
}

- (void)setPausesLocationUpdatesAutomatically:(BOOL)pauses
{
}

- (BOOL)pausesLocationUpdatesAutomatically
{
	return NO;
}

- (void)setAllowsBackgroundLocationUpdates:(BOOL)allows
{
}

- (BOOL)allowsBackgroundLocationUpdates
{
	return NO;
}

- (void)setShowsBackgroundLocationIndicator:(BOOL)shows
{
}

- (BOOL)showsBackgroundLocationIndicator
{
	return NO;
}

- (void)setActivityType:(NSInteger)type
{
}

- (NSInteger)activityType
{
	return 1;
}

- (BOOL)locationServicesEnabled
{
	return NO;
}

@end
