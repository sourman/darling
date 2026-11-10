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

/* Comet dyld stubs. Symbols exist so the process can load. */
const CFStringRef kTISPropertyInputSourceID = CFSTR("TISPropertyInputSourceID");
const CFStringRef kTISPropertyInputSourceType = CFSTR("TISPropertyInputSourceType");
const CFStringRef kTISPropertyUnicodeKeyLayoutData = CFSTR("TISPropertyUnicodeKeyLayoutData");
const CFStringRef kTISTypeKeyboardLayout = CFSTR("TISTypeKeyboardLayout");
__attribute__((visibility("default"), used)) long CallNextEventHandler(void) { return 0; }
__attribute__((visibility("default"), used)) long GetCurrentKeyModifiers(void) { return 0; }
__attribute__((visibility("default"), used)) long GetEventKind(void) { return 0; }
__attribute__((visibility("default"), used)) long GetEventParameter(void) { return 0; }
__attribute__((visibility("default"), used)) long InstallEventHandler(void) { return 0; }
__attribute__((visibility("default"), used)) long LMGetKbdType(void) { return 0; }
__attribute__((visibility("default"), used)) long ReceiveNextEvent(void) { return 0; }
__attribute__((visibility("default"), used)) long RegisterEventHotKey(void) { return 0; }
__attribute__((visibility("default"), used)) long ReleaseEvent(void) { return 0; }
__attribute__((visibility("default"), used)) long RemoveEventHandler(void) { return 0; }
TISInputSourceRef TISCopyCurrentASCIICapableKeyboardLayoutInputSource(void) { return NULL; }
TISInputSourceRef TISCopyCurrentKeyboardLayoutInputSource(void) { return NULL; }
TISInputSourceRef TISCopyCurrentKeyboardInputSource(void) { return NULL; }
void *TISGetInputSourceProperty(TISInputSourceRef inputSourceRef, CFStringRef key) { (void)inputSourceRef; (void)key; return NULL; }
__attribute__((visibility("default"), used)) long UnregisterEventHotKey(void) { return 0; }
