cmake_minimum_required(VERSION 3.10)
project(create_disp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(create-disp create-disp.cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

target_include_directories(create-disp PRIVATE
    ${SYSTEMD_INCLUDE_DIRS}
    /usr/include/android
    /usr/include/linux
)

target_link_libraries(create-disp
    hwc2
    hybris-common
    hybris-eglplatformcommon
    hybris-platformcommon
    gralloc
    ${SYSTEMD_LIBRARIES}
)
