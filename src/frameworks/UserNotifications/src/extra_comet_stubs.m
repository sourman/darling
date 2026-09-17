/* Comet startup cluster: UserNotifications class methods and completion APIs. */
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSError.h>
#import <UserNotifications/UNUserNotificationCenter.h>
#import <UserNotifications/UNNotificationSettings.h>

NSString *UNNotificationAttachmentOptionsTypeHintKey = @"UNNotificationAttachmentOptionsTypeHintKey";

const char comet_startup_cluster_stubs_v1[] = "comet_startup_cluster_stubs_v1";

enum {
	UNAuthorizationStatusNotDetermined = 0,
	UNAuthorizationStatusDenied = 1,
	UNAuthorizationStatusAuthorized = 2
};

enum {
	UNNotificationSettingNotSupported = 0,
	UNNotificationSettingDisabled = 1,
	UNNotificationSettingEnabled = 2
};

@implementation UNNotificationSettings (CometStartupCluster)
- (NSInteger)authorizationStatus
{
	return UNAuthorizationStatusDenied;
}
- (NSInteger)soundSetting
{
	return UNNotificationSettingDisabled;
}
- (NSInteger)badgeSetting
{
	return UNNotificationSettingDisabled;
}
- (NSInteger)alertSetting
{
	return UNNotificationSettingDisabled;
}
- (NSInteger)notificationCenterSetting
{
	return UNNotificationSettingDisabled;
}
- (NSInteger)lockScreenSetting
{
	return UNNotificationSettingDisabled;
}
- (NSInteger)carPlaySetting
{
	return UNNotificationSettingNotSupported;
}
- (NSInteger)alertStyle
{
	return 0;
}
- (NSInteger)showPreviewsSetting
{
	return 2; /* Never */
}
- (NSInteger)criticalAlertSetting
{
	return UNNotificationSettingNotSupported;
}
- (BOOL)providesAppNotificationSettings
{
	return NO;
}
- (NSInteger)announcementSetting
{
	return UNNotificationSettingNotSupported;
}
- (NSInteger)timeSensitiveSetting
{
	return UNNotificationSettingDisabled;
}
- (NSInteger)scheduledDeliverySetting
{
	return UNNotificationSettingDisabled;
}
- (NSInteger)directMessagesSetting
{
	return UNNotificationSettingDisabled;
}
@end

@implementation UNUserNotificationCenter (CometStartupCluster)

+ (instancetype)currentNotificationCenter
{
	static UNUserNotificationCenter *center = nil;
	if (center == nil)
		center = [[UNUserNotificationCenter alloc] init];
	return center;
}

+ (BOOL)supportsContentExtensions
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

- (void)getNotificationSettingsWithCompletionHandler:(void (^)(UNNotificationSettings *settings))completionHandler
{
	if (completionHandler)
		completionHandler([[[UNNotificationSettings alloc] init] autorelease]);
}

- (void)requestAuthorizationWithOptions:(NSUInteger)options
                      completionHandler:(void (^)(BOOL granted, NSError *error))completionHandler
{
	if (completionHandler)
		completionHandler(NO, nil);
}

- (void)addNotificationRequest:(id)request
         withCompletionHandler:(void (^)(NSError *error))completionHandler
{
	if (completionHandler)
		completionHandler(nil);
}

- (void)getDeliveredNotificationsWithCompletionHandler:(void (^)(NSArray *notifications))completionHandler
{
	if (completionHandler)
		completionHandler([NSArray array]);
}

- (void)removeDeliveredNotificationsWithIdentifiers:(NSArray *)identifiers
{
}

- (void)removeAllDeliveredNotifications
{
}

- (void)getPendingNotificationRequestsWithCompletionHandler:(void (^)(NSArray *requests))completionHandler
{
	if (completionHandler)
		completionHandler([NSArray array]);
}

- (void)removePendingNotificationRequestsWithIdentifiers:(NSArray *)identifiers
{
}

- (void)removeAllPendingNotificationRequests
{
}

- (void)setNotificationCategories:(id)categories
{
}

- (void)getNotificationCategoriesWithCompletionHandler:(void (^)(id categories))completionHandler
{
	if (completionHandler)
		completionHandler([NSArray array]);
}

- (NSString *)delegateQueue
{
	return nil;
}

@end
