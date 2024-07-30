#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(velopera_nrf_firmware)

#Including local extra cmake modules
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(GenerateExportHeader)

# Include files that are common for all modules
add_subdirectory(src/common)

# Include mandatory module source folders
add_subdirectory(src/modules/trigger)
add_subdirectory(src/modules/fota)
add_subdirectory(src/modules/network)
add_subdirectory(src/modules/transport)
add_subdirectory(src/modules/error)
add_subdirectory(src/modules/location)


include_directories(
        "${CMAKE_CURRENT_BINARY_DIR}/generated/"
        )

################################################################################
#Version stuff
################################################################################

# Make a version file containing the current version from git.
include(GetGitRevisionDescription)
git_describe(VERSION --tag --long)

set(PROJECT_DESCRIPTION "VELOpera Firmware")


##parse the version information into pieces.
string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" PROJECT_VERSION_MAJOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" PROJECT_VERSION_MINOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.([0-9]+)-.*" "\\1" PROJECT_VERSION_PATCH "${VERSION}")
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(PROJECT_VERSION_FULL "${VERSION}")

message("PVS " ${VERSION})
message("PMJ " ${PROJECT_VERSION_MAJOR})
message("PMN " ${PROJECT_VERSION_MINOR})
message("PPT " ${PROJECT_VERSION_PATCH})
message("PFL " ${PROJECT_VERSION_FULL}) 


message (STATUS "version: " ${VERSION} )
message (STATUS ${PROJECT_NAME}" version: " ${PROJECT_VERSION_FULL} )

set(DEVEL_USER $ENV{USER})
set(USER_${DEVEL_USER} 1)

################################################################################
# Install options
################################################################################
MESSAGE(STATUS "CMAKE_INSTALL_PREFIX: " ${CMAKE_INSTALL_PREFIX})

set(INSTALL_LIB_DIR lib/ CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin/ CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include/${PROJECT_NAME} CACHE PATH "Installation directory for header files")
if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR cmake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib/cmake/${PROJECT_NAME})
endif()
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")


#
# set these early to override pollution by installed versions
#
include_directories(
        "${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}"
        "${PROJECT_BINARY_DIR}/generated/velopera-nrf-firmware"        
        )


# #Build target dir for config, export and package files
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")


# Generate Config file toffy_config.h
#set(config_h_in "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}_config.h.in")
#set(config_h "${generated_dir}/${PROJECT_NAME}/${PROJECT_NAME}_config.h")
#configure_file("${config_h_in}" "${config_h}")
#list(APPEND headers ${config_h})
#install(
#   FILES ${config_h}
#   DESTINATION "${INSTALL_INCLUDE_DIR}/common"
#)

# Generate Config file toffy_config.h
set(config_h_in "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}_config.h.in")
set(config_h "${generated_dir}/${PROJECT_NAME}/${PROJECT_NAME}_config.h")
configure_file("${config_h_in}" "${config_h}")
list(APPEND headers ${config_h})
install(
   FILES ${config_h}
   DESTINATION "${INSTALL_INCLUDE_DIR}/common"
)

message (STATUS "version: " ${VERSION} )
message (STATUS ${PROJECT_NAME}" version: " ${PROJECT_VERSION_FULL} )