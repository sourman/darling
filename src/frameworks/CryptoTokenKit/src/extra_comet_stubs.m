/* Comet/Chrome CryptoTokenKit compatibility: no smart-card tokens. */
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>

@interface TKTokenWatcher : NSObject
- (instancetype)init;
- (instancetype)initWithInsertionHandler:(id)insertionHandler;
- (void)setInsertionHandler:(id)insertionHandler;
- (void)addRemovalHandler:(id)removalHandler forTokenID:(id)tokenID;
- (NSArray *)tokenIDs;
@end

@implementation TKTokenWatcher

- (instancetype)init
{
	return [super init];
}

- (instancetype)initWithInsertionHandler:(id)insertionHandler
{
	(void)insertionHandler;
	return [self init];
}

- (void)setInsertionHandler:(id)insertionHandler
{
	(void)insertionHandler;
}

- (void)addRemovalHandler:(id)removalHandler forTokenID:(id)tokenID
{
	(void)removalHandler;
	(void)tokenID;
}

- (NSArray *)tokenIDs
{
	return [NSArray array];
}

@end
