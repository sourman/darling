/* Comet compatibility stubs. Symbols exist so dyld can bind; behavior is not implemented. */
__attribute__((visibility("default"), used)) void *ASAuthorizationErrorDomain = 0;
__attribute__((visibility("default"), used)) void ASAuthorizationPublicKeyCredentialAttestationKindNone(void) {}
__attribute__((visibility("default"), used)) void ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged(void) {}
__attribute__((visibility("default"), used)) void ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred(void) {}
__attribute__((visibility("default"), used)) void ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired(void) {}
__attribute__((visibility("default"), used)) void *ASWebAuthenticationSessionErrorDomain = 0;

#define COMET_EMPTY_CLASS(Name) \
static const char comet_name_##Name[] = #Name; \
static struct { unsigned flags, start, size, reserved; void *ivarLayout; const char *name; void *methods, *protos, *ivars, *weak, *props; } \
  comet_ro_##Name = { 2u, 0u, 8u, 0u, 0, comet_name_##Name, 0,0,0,0,0 }, \
  comet_meta_ro_##Name = { 3u, 0u, 40u, 0u, 0, comet_name_##Name, 0,0,0,0,0 }; \
struct comet_class_##Name { void *w[32]; }; \
extern struct comet_class_##Name OBJC_METACLASS_$_##Name; \
__attribute__((visibility("default"), used, aligned(8))) struct comet_class_##Name OBJC_CLASS_$_##Name = {{ \
  (void*)&OBJC_METACLASS_$_##Name, 0, 0, 0, (void*)&comet_ro_##Name \
}}; \
__attribute__((visibility("default"), used, aligned(8))) struct comet_class_##Name OBJC_METACLASS_$_##Name = {{ \
  (void*)&OBJC_METACLASS_$_##Name, (void*)&OBJC_CLASS_$_##Name, 0, 0, (void*)&comet_meta_ro_##Name \
}};

COMET_EMPTY_CLASS(ASAuthorizationPublicKeyCredentialLargeBlobAssertionInput)
COMET_EMPTY_CLASS(ASAuthorizationPublicKeyCredentialLargeBlobRegistrationInput)
COMET_EMPTY_CLASS(ASAuthorizationPublicKeyCredentialPRFAssertionInput)
COMET_EMPTY_CLASS(ASAuthorizationPublicKeyCredentialPRFAssertionInputValues)
COMET_EMPTY_CLASS(ASAuthorizationPublicKeyCredentialPRFRegistrationInput)
COMET_EMPTY_CLASS(ASAuthorizationWebBrowserPublicKeyCredentialManager)
