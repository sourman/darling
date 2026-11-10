/* Comet compatibility stubs. Symbols exist so dyld can bind; behavior is not implemented. */
__attribute__((visibility("default"), used)) void AECreateAppleEvent(void) {}
__attribute__((visibility("default"), used)) void AEGetParamDesc(void) {}
__attribute__((visibility("default"), used)) void AEPutParamDesc(void) {}
__attribute__((visibility("default"), used)) void AERemoveEventHandler(void) {}
__attribute__((visibility("default"), used)) void AESendMessage(void) {}
__attribute__((visibility("default"), used)) void FSEventsGetCurrentEventId(void) {}
__attribute__((visibility("default"), used)) void LSSharedFileListCopySnapshot(void) {}
__attribute__((visibility("default"), used)) void LSSharedFileListCreate(void) {}
__attribute__((visibility("default"), used)) void LSSharedFileListInsertItemURL(void) {}
__attribute__((visibility("default"), used)) void LSSharedFileListItemCopyProperty(void) {}
__attribute__((visibility("default"), used)) void LSSharedFileListItemCopyResolvedURL(void) {}
__attribute__((visibility("default"), used)) void LSSharedFileListItemRemove(void) {}
__attribute__((visibility("default"), used)) void _CSCheckFixDisable(void) {}
__attribute__((visibility("default"), used)) void *_kLSOpenOptionBackgroundLaunchKey = 0;

/* Comet dyld stubs. Symbols exist so the process can load. */
__attribute__((visibility("default"), used)) void *kLSQuarantineDataURLKey = 0;
__attribute__((visibility("default"), used)) void *kLSQuarantineOriginURLKey = 0;
__attribute__((visibility("default"), used)) void *kLSQuarantineTypeKey = 0;
__attribute__((visibility("default"), used)) void *kLSQuarantineTypeOtherDownload = 0;
__attribute__((visibility("default"), used)) void *kLSQuarantineTypeWebDownload = 0;
__attribute__((visibility("default"), used)) void *kLSSharedFileListItemLast = 0;
__attribute__((visibility("default"), used)) void *kLSSharedFileListLoginItemHidden = 0;
__attribute__((visibility("default"), used)) void *kLSSharedFileListSessionLoginItems = 0;
__attribute__((visibility("default"), used)) void *kMDItemLastUsedDate = 0;
__attribute__((visibility("default"), used)) void *kMDItemWhereFroms = 0;
__attribute__((visibility("default"), used)) long AECreateDesc(void) { return 0; }
__attribute__((visibility("default"), used)) long AEDisposeDesc(void) { return 0; }
__attribute__((visibility("default"), used)) long AEInstallEventHandler(void) { return 0; }
__attribute__((visibility("default"), used)) long FSEventStreamCreate(void) { return 0; }
__attribute__((visibility("default"), used)) long FSEventStreamGetLatestEventId(void) { return 0; }
__attribute__((visibility("default"), used)) long FSEventStreamInvalidate(void) { return 0; }
__attribute__((visibility("default"), used)) long FSEventStreamRelease(void) { return 0; }
__attribute__((visibility("default"), used)) long FSEventStreamSetDispatchQueue(void) { return 0; }
__attribute__((visibility("default"), used)) long FSEventStreamStart(void) { return 0; }
__attribute__((visibility("default"), used)) long FSEventStreamStop(void) { return 0; }
__attribute__((visibility("default"), used)) long LSOpenCFURLRef(void) { return 0; }
__attribute__((visibility("default"), used)) long LSOpenFromURLSpec(void) { return 0; }
__attribute__((visibility("default"), used)) long LSRegisterURL(void) { return 0; }
__attribute__((visibility("default"), used)) long MDItemCopyAttribute(void) { return 0; }
__attribute__((visibility("default"), used)) long MDItemCreate(void) { return 0; }
__attribute__((visibility("default"), used)) long MDItemSetAttribute(void) { return 0; }
__attribute__((visibility("default"), used)) long UCKeyTranslate(void) { return 0; }
__attribute__((visibility("default"), used)) long _LSSetApplicationLaunchServicesServerConnectionStatus(void) { return 0; }
