cmake_minimum_required (VERSION 2.8)

include (platform)
initialize_platform()

include (platform/install)

find_package (vfs REQUIRED)

add_subdirectory (src)
add_subdirectory (einit)
