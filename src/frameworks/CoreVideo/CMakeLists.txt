project(CoreVideo)

remove_sdk_framework(CoreVideo)

set(DYLIB_COMPAT_VERSION "1.2.0")
set(DYLIB_CURRENT_VERSION "1.5.0")

set(FRAMEWORK_VERSION "A")

include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})

generate_sdk_framework(CoreVideo
    VERSION ${FRAMEWORK_VERSION}
    HEADER "include/CoreVideo"
)

add_framework(CoreVideo
    FAT
    CURRENT_VERSION
    VERSION ${FRAMEWORK_VERSION}

    SOURCES
	    src/constants.c
	    src/stubs.c
        src/CVDisplayLink.mm

    DEPENDENCIES
        system
        Foundation
        CoreGraphics
        cxx
)


# Comet dyld compatibility stubs
if(TARGET CoreVideo_obj)
  target_sources(CoreVideo_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/comet_stubs.m)
elseif(TARGET CoreVideo)
  target_sources(CoreVideo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/comet_stubs.m)
endif()
