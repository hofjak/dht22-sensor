set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Optial tools directory. 
# Comment this line if you don't need it.
set(CROSS_TOOLS_DIR "/path/to/toolchain/root/directory")

set(KERNEL_RELEASE "5.15.y")

# Path to kernel build directory. Contains the Makefile.
set(KERNEL_BUILD_DIR "${CROSS_TOOLS_DIR}/kernels/${KERNEL_RELEASE}")

# Path to the kernel headers. 
# Might be different than the kernel build directory.
set(KERNEL_HEADERS_DIR "${CROSS_TOOLS_DIR}/kernels/${KERNEL_RELEASE}")

# Specify cross compiler
set(CMAKE_C_COMPILER "${CROSS_TOOLS_DIR}/compilers/cross-pi-gcc-10.2.0-2/bin/arm-linux-gnueabihf-gcc")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
