project(MediaAccessibility)

remove_sdk_framework(MediaAccessibility)

set(DYLIB_COMPAT_VERSION "1.0.0")
set(DYLIB_CURRENT_VERSION "62.0.0")

set(FRAMEWORK_VERSION "A")

generate_sdk_framework(MediaAccessibility
    VERSION ${FRAMEWORK_VERSION}
    HEADER "include/MediaAccessibility"
)

add_framework(MediaAccessibility
    FAT
    CURRENT_VERSION
    VERSION ${FRAMEWORK_VERSION}

    SOURCES
        src/MediaAccessibility.c

    DEPENDENCIES
        system
)

# Comet dyld compatibility stubs
if(TARGET MediaAccessibility_obj)
  target_sources(MediaAccessibility_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/comet_stubs.m)
elseif(TARGET MediaAccessibility)
  target_sources(MediaAccessibility PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/comet_stubs.m)
endif()
