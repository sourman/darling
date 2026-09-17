#include <CoreFoundation/CoreFoundation.h>

void *VTRegisterSupplementalVideoDecoderIfAvailable(void) { return 0; }

const CFStringRef kVTCompressionPropertyKey_BaseLayerFrameRateFraction = CFSTR("BaseLayerFrameRateFraction");
const CFStringRef kVTCompressionPropertyKey_CalculateMeanSquaredError = CFSTR("CalculateMeanSquaredError");
const CFStringRef kVTCompressionPropertyKey_ColorPrimaries = CFSTR("ColorPrimaries");
const CFStringRef kVTCompressionPropertyKey_EncoderID = CFSTR("EncoderID");
const CFStringRef kVTCompressionPropertyKey_ReferenceBufferCount = CFSTR("ReferenceBufferCount");
const CFStringRef kVTCompressionPropertyKey_TransferFunction = CFSTR("TransferFunction");
const CFStringRef kVTCompressionPropertyKey_YCbCrMatrix = CFSTR("YCbCrMatrix");
const CFStringRef kVTEncodeFrameOptionKey_BaseFrameQP = CFSTR("BaseFrameQP");
const CFStringRef kVTProfileLevel_HEVC_Main_AutoLevel = CFSTR("HEVC_Main_AutoLevel");
const CFStringRef kVTSampleAttachmentKey_QualityMetrics = CFSTR("QualityMetrics");
const CFStringRef kVTSampleAttachmentQualityMetricsKey_ChromaBlueMeanSquaredError = CFSTR("ChromaBlueMeanSquaredError");
const CFStringRef kVTSampleAttachmentQualityMetricsKey_ChromaRedMeanSquaredError = CFSTR("ChromaRedMeanSquaredError");
const CFStringRef kVTSampleAttachmentQualityMetricsKey_LumaMeanSquaredError = CFSTR("LumaMeanSquaredError");
const CFStringRef kVTVideoEncoderSpecification_EnableLowLatencyRateControl = CFSTR("EnableLowLatencyRateControl");
