cmake_minimum_required(VERSION 3.16)
project(streamdeckpp VERSION 1.8 LANGUAGES CXX)

# --- Options ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ABI 1)
set(PUBLIC_HEADER streamdeckpp.hh)

# --- Find dependencies using pkg-config ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi-libusb)
pkg_check_modules(MAGICKPP REQUIRED IMPORTED_TARGET Magick++)

# --- Include directories ---
include_directories(
    ${HIDAPI_INCLUDE_DIRS}
    ${MAGICKPP_INCLUDE_DIRS}
)

# --- Sources ---
set(LIB_SRCS streamdeckpp.cc)
set(MAIN_SRCS main.cc)

# --- Static Library ---
add_library(streamdeckpp_static STATIC ${LIB_SRCS})
target_link_libraries(streamdeckpp_static PUBLIC PkgConfig::HIDAPI PkgConfig::MAGICKPP)
set_target_properties(streamdeckpp_static PROPERTIES OUTPUT_NAME streamdeckpp)
target_include_directories(streamdeckpp_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- Shared Library ---
add_library(streamdeckpp SHARED streamdeckpp.cc)
target_link_libraries(streamdeckpp PUBLIC PkgConfig::HIDAPI PkgConfig::MAGICKPP)
set_target_properties(streamdeckpp PROPERTIES
    SOVERSION ${ABI}
    VERSION ${PROJECT_VERSION}
    OUTPUT_NAME streamdeckpp
    PUBLIC_HEADER ${PUBLIC_HEADER}
)
# Optionally add linker script if needed
target_link_options(streamdeckpp PRIVATE LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libstreamdeckpp.map)

# --- Executable ---
add_executable(streamdeck ${MAIN_SRCS})
target_link_libraries(streamdeck PRIVATE streamdeckpp_static)
target_include_directories(streamdeck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# --- Install Rules ---
include(GNUInstallDirs)

install(TARGETS streamdeckpp streamdeckpp_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/streamdeckpp-${PROJECT_VERSION}
)

install(TARGETS streamdeck
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${PUBLIC_HEADER} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/streamdeckpp-${PROJECT_VERSION})

# --- pkg-config file generation ---
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/streamdeckpp.spec.in
    ${CMAKE_CURRENT_BINARY_DIR}/streamdeckpp.pc
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/streamdeckpp.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# --- Clean/dist targets (CMake handles most) ---

# --- Custom targets for rpm, srpm, dist (optional, not implemented here) ---
