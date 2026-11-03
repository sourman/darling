/* Comet compatibility stubs. Symbols exist so dyld can bind; behavior is not implemented. */
#include <CoreFoundation/CFArray.h>
#include <HIToolbox/TextInputSources.h>

__attribute__((visibility("default"), used)) void AEProcessEvent(void) {}
__attribute__((visibility("default"), used)) void DisableSecureEventInput(void) {}
__attribute__((visibility("default"), used)) void EnableSecureEventInput(void) {}
__attribute__((visibility("default"), used)) void LMGetKbdLast(void) {}

static CFArrayRef TISCreateOneSourceList(void) {
    TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
    if (!source)
        return CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
    const void *value = source;
    CFArrayRef list = CFArrayCreate(NULL, &value, 1, &kCFTypeArrayCallBacks);
    CFRelease(source);
    return list;
}

__attribute__((visibility("default"), used))
CFArrayRef TISCreateASCIICapableInputSourceList(void) {
    return TISCreateOneSourceList();
}

__attribute__((visibility("default"), used))
CFArrayRef TISCreateInputSourceList(CFDictionaryRef properties, Boolean includeAllInstalled) {
    (void)properties;
    (void)includeAllInstalled;
    return TISCreateOneSourceList();
}
__attribute__((visibility("default"), used)) void TSMRemoveDocumentProperty(void) {}
__attribute__((visibility("default"), used)) void TSMSetDocumentProperty(void) {}
