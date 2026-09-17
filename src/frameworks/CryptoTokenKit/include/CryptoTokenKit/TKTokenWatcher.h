/* Public TKTokenWatcher surface used by Chrome/Comet. No tokens on Darling. */

@interface TKTokenWatcher : NSObject

@property (readonly) NSArray *tokenIDs;

- (id)init;
- (id)initWithInsertionHandler:(id /* block */)arg1;
- (void)setInsertionHandler:(id /* block */)arg1;
- (void)addRemovalHandler:(id /* block */)arg1 forTokenID:(id)arg2;
- (id)tokenIDs;

@end
