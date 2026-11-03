/* Comet compatibility stubs. Symbols exist so dyld can bind; behavior is not implemented. */
#import <Foundation/Foundation.h>

@interface TKTokenWatcher : NSObject
- (instancetype)initWithInsertionHandler:(void (^)(id tokenID))handler;
- (void)addRemovalHandler:(void (^)(id tokenID))handler forTokenID:(id)tokenID;
@property (readonly) NSArray *tokenIDs;
@end

@implementation TKTokenWatcher
- (instancetype)initWithInsertionHandler:(void (^)(id) )handler {
    return [super init];
}
- (void)addRemovalHandler:(void (^)(id) )handler forTokenID:(id)tokenID {
}
- (NSArray *)tokenIDs {
    return @[];
}
@end
