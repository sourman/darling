project(AudioToolbox)

include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../include
	${CMAKE_CURRENT_SOURCE_DIR}/../include/AudioToolbox
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

set(audiotoolbox_sources
	AUComponent.cpp
	AudioConverter.cpp
	AudioConverterImpl.cpp
	AudioFile.cpp
	AudioQueueBase.cpp
	AudioQueue.cpp
	AudioServices.cpp
	AudioQueueOutput.cpp
	AUGraph.cpp
	AudioComponent.mm
	AudioComponentManager.mm
	AudioFileComponent.cpp
	AudioFileFormatManager.cpp
	ExtendedAudioFile.cpp
	AudioFormat.cpp
	MusicDevice.cpp
	AudioToolbox.cpp
	AudioHardwareService.cpp
)

add_framework(AudioToolbox
	FAT
	CURRENT_VERSION
    VERSION ${FRAMEWORK_AUDIOTOOLBOX_VERSION}

    SOURCES
        ${audiotoolbox_sources}

    DEPENDENCIES
        system
        cxx
		CoreFoundation
		CoreAudio
		CoreServices
		objc
		avformat
		swresample
		avcodec
		avutil
)

add_framework(AudioUnit
	FAT
	CURRENT_VERSION
	VERSION ${FRAMEWORK_AUDIOUNIT_VERSION}

	SOURCES
		empty.c

	DEPENDENCIES
		AudioToolbox
)
set(AudioToolbox_BUILD ${CMAKE_CURRENT_BINARY_DIR}/AudioToolbox)

reexport(AudioUnit AudioToolbox ${AudioToolbox_BUILD})

set_property(TARGET AudioToolbox APPEND_STRING PROPERTY LINK_FLAGS
  " -Wl,-exported_symbol,_AudioComponentFindNext -Wl,-exported_symbol,_AudioComponentInstanceDispose -Wl,-exported_symbol,_AudioComponentInstanceNew -Wl,-exported_symbol,_AudioOutputUnitStart -Wl,-exported_symbol,_AudioOutputUnitStop -Wl,-exported_symbol,_AudioUnitGetProperty -Wl,-exported_symbol,_AudioUnitGetPropertyInfo -Wl,-exported_symbol,_AudioUnitInitialize -Wl,-exported_symbol,_AudioUnitRender -Wl,-exported_symbol,_AudioUnitReset -Wl,-exported_symbol,_AudioUnitSetProperty -Wl,-exported_symbol,_AudioUnitUninitialize -Wl,-exported_symbol,_AudioConverterDispose -Wl,-exported_symbol,_AudioConverterFillComplexBuffer -Wl,-exported_symbol,_AudioConverterGetProperty -Wl,-exported_symbol,_AudioConverterGetPropertyInfo -Wl,-exported_symbol,_AudioConverterNew -Wl,-exported_symbol,_AudioConverterReset -Wl,-exported_symbol,_AudioConverterSetProperty -Wl,-exported_symbol,_AudioFileClose -Wl,-exported_symbol,_AudioFileInitializeWithCallbacks -Wl,-exported_symbol,_AudioFileSetProperty -Wl,-exported_symbol,_AudioFileWritePackets -Wl,-exported_symbol,_AudioFormatGetProperty -Wl,-exported_symbol,_AudioFormatGetPropertyInfo -Wl,-exported_symbol,_AudioQueueAllocateBuffer -Wl,-exported_symbol,_AudioQueueDispose -Wl,-exported_symbol,_AudioQueueEnqueueBuffer -Wl,-exported_symbol,_AudioQueueNewInput -Wl,-exported_symbol,_AudioQueueStart -Wl,-exported_symbol,_AudioQueueStop")


# Comet dyld compatibility stubs
if(TARGET AudioToolbox_obj)
  target_sources(AudioToolbox_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/comet_stubs.m)
elseif(TARGET AudioToolbox)
  target_sources(AudioToolbox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/comet_stubs.m)
endif()
