/* Comet compatibility stubs. Symbols exist so dyld can bind; behavior is not implemented. */
__attribute__((visibility("default"), used)) void vDSP_vsadd(void) {}

/* Comet dyld stubs. Symbols exist so the process can load. */
__attribute__((visibility("default"), used)) long vDSP_conv(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_create_fftsetup(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_ctoz(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_deq22D(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_destroy_fftsetup(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_fft_zrip(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_maxmgv(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_svesq(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_vadd(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_vclip(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_vmul(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_vsma(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_vsmul(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_vsub(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_ztoc(void) { return 0; }
__attribute__((visibility("default"), used)) long vDSP_zvmul(void) { return 0; }
