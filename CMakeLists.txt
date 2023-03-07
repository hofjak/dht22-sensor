cmake_minimum_required(VERSION 3.18.4 FATAL_ERROR)


project(dht22 VERSION 1.0 LANGUAGES C)


if (NOT "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    message(FATAL_ERROR "Can't compile kernel module with a compiler other than gcc")
endif()


if (CMAKE_CROSSCOMPILING)
    message(STATUS "Crosscompiling for ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_SYSTEM_NAME}")
endif()


list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include("${CMAKE_SOURCE_DIR}/cmake/Utils.cmake")


# Promote the use of a tempfs for building
check_and_warn_tmpfs()


# Find kernel headers
find_package(KernelBuildEnv REQUIRED)


add_definitions(-D__KERNEL__ -DMODULE -DKBUILD_MODNAME="${PROJECT_NAME}" -DDEBUG)


# This is needed in order for IDEs to provide syntax highlightning.
# The executable name is independent from the actual module that is built.
add_executable("dummy" "src/${PROJECT_NAME}.c")
target_include_directories("dummy" PRIVATE "${KERNEL_HEADERS_INCLUDE_DIRS}")


add_subdirectory(src)
add_subdirectory(examples)


set(MAKE_CROSS_COMPILE_ARGUMENTS "")


if (CMAKE_CROSSCOMPILING)
    string(REGEX REPLACE "gcc$" "" TMP_CC "${CMAKE_C_COMPILER}") # gcc gets reappended in Kbuild system
    set(MAKE_CROSS_COMPILE_ARGUMENTS ARCH=${CMAKE_SYSTEM_PROCESSOR} CROSS_COMPILE=${TMP_CC})
endif()


set(MAKE_ARGUMENTS
    $<$<CONFIG:Debug>:KCFLAGS=-DDEBUG> 
    ${MAKE_CROSS_COMPILE_ARGUMENTS} -C ${KERNEL_BUILD_DIR} M=${CMAKE_BINARY_DIR}/src
    CACHE STRING  "Arguments needed for Kbuild"
)


set(CMAKE_VERBOSE_MAKEFILE ON)


add_custom_target(build-module
    COMMAND make ${MAKE_ARGUMENTS} modules
    COMMENT "Building module '${PROJECT_NAME}'..."
)


add_custom_target(clean-module
    COMMAND make ${MAKE_ARGUMENTS} clean
    COMMENT "Clean build files for '${PROJECT_NAME}'..."
)


add_custom_target(build-examples
    COMMAND make -C ${CMAKE_BINARY_DIR}/examples
    COMMENT "Build examples'..."
)
