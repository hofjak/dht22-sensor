message(STATUS "Set up kernel build environment")


if (NOT DEFINED KERNEL_RELEASE)
    execute_process(COMMAND uname -r
        OUTPUT_VARIABLE KERNEL_RELEASE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()


message(STATUS "Kernel release: ${KERNEL_RELEASE}")


if (NOT DEFINED KERNEL_BUILD_DIR)
    set(KERNEL_BUILD_DIR "/usr/lib/modules/${KERNEL_RELEASE}/build")
endif()


find_file(KERNEL_MAKEFILE
    NAMES Makefile
    PATHS "${KERNEL_BUILD_DIR}" NO_DEFAULT_PATH
)


if (NOT KERNEL_MAKEFILE)
    message(FATAL_ERROR "No Makefile in ${KERNEL_BUILD_DIR}")
endif()


message(STATUS "Found Makefile: ${KERNEL_MAKEFILE}")


if (NOT DEFINED KERNEL_HEADERS_DIR)
    set(KERNEL_HEADERS_DIR "/usr/lib/modules/${KERNEL_RELEASE}/build")
endif()


# Find the headers
find_path(KERNEL_HEADERS_ENV
    "include/linux/user.h"
    PATHS "${KERNEL_HEADERS_DIR}"
)


if (KERNEL_HEADERS_ENV)
    set(KERNEL_HEADERS_INCLUDE_DIRS
        # Not checked if they really exist
        ${KERNEL_HEADERS_DIR}/include
        ${KERNEL_HEADERS_DIR}/arch/${CMAKE_SYSTEM_PROCESSOR}/include
        ${KERNEL_HEADERS_DIR}/arch/${CMAKE_SYSTEM_PROCESSOR}/include/asm
        ${KERNEL_HEADERS_DIR}/arch/${CMAKE_SYSTEM_PROCESSOR}/include/generated
        ${KERNEL_HEADERS_DIR}/arch/${CMAKE_SYSTEM_PROCESSOR}/include/uapi
        ${KERNEL_HEADERS_DIR}/arch/${CMAKE_SYSTEM_PROCESSOR}/include/generated/uapi
        CACHE PATH "Kernel headers include directories"
    )
else()
    message(FATAL_ERROR "Can't find kernel headers: ${KERNEL_HEADERS_DIR}")
endif()


message(STATUS "Found kernel headers: ${KERNEL_HEADERS_DIR}")


message(STATUS "Set up kernel build environment - done")
