/* Comet compatibility stubs. Symbols exist so dyld can bind; behavior is not implemented. */
__attribute__((visibility("default"), used)) void DADiskClaim(void) {}
__attribute__((visibility("default"), used)) void DADiskEject(void) {}
__attribute__((visibility("default"), used)) void DADiskUnclaim(void) {}
__attribute__((visibility("default"), used)) void DADiskUnmount(void) {}
__attribute__((visibility("default"), used)) void DARegisterDiskAppearedCallback(void) {}
__attribute__((visibility("default"), used)) void DARegisterDiskDescriptionChangedCallback(void) {}
__attribute__((visibility("default"), used)) void DARegisterDiskDisappearedCallback(void) {}
__attribute__((visibility("default"), used)) void *kDADiskDescriptionMatchVolumeMountable = 0;
__attribute__((visibility("default"), used)) void *kDADiskDescriptionWatchVolumePath = 0;
