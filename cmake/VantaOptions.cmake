option(BUILD_VANTA_SHARED "Build Vanta shared libraries" OFF)
option(BUILD_VANTA_STATIC "Build Vanta static libraries" ON)
option(BUILD_VANTA_TESTS "Build Vanta tests" ON)
option(VANTA_ENABLE_WARNINGS "Enable compiler warnings" ON)

if(NOT BUILD_VANTA_SHARED AND NOT BUILD_VANTA_STATIC)
    message(FATAL_ERROR "At least one of BUILD_VANTA_SHARED or BUILD_VANTA_STATIC must be ON")
endif()

set(VANTA_CORE_SHARED_TARGET vanta_core_shared)
set(VANTA_CORE_STATIC_TARGET vanta_core_static)
set(VANTA_CORE_SHARED_OUTPUT_NAME vanta_core)
set(VANTA_CORE_STATIC_OUTPUT_NAME vanta_core_static)
