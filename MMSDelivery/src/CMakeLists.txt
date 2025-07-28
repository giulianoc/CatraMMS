# Copyright (C) Giuliano Catrambone (giuliano.catrambone@catrasoftware.it)

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.

# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.

# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 675 Mass
# Ave, Cambridge, MA 02139, USA.

# Commercial use other than under the terms of the GNU General Public License is
# allowed only after express negotiation of conditions with the authors.

set(SOURCES
  HostBandwidthTracker.cpp
  MMSDeliveryAuthorization.cpp
)
set(HEADERS
  HostBandwidthTracker.h
  MMSDeliveryAuthorization.h
)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(
  COMMAND ${LSB_RELEASE_EXEC} -is
  OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
  OUTPUT_STRIP_TRAILING_WHITESPACE)

include_directories("${STRINGUTILS_INCLUDE_DIR}")
#include_directories("${DATETIME_INCLUDE_DIR}")
include_directories("${ENCRYPT_INCLUDE_DIR}")
#include_directories("${SYSTEM_INCLUDE_DIR}")
include_directories("${CONVERT_INCLUDE_DIR}")
include_directories("${JSONUTILS_INCLUDE_DIR}")
#include_directories("${MULTIEVENTSSET_INCLUDE_DIR}")
include_directories("${POSTGRESHELPER_INCLUDE_DIR}")
include_directories("${DBCONN_INCLUDE_DIR}")
include_directories("${CURLWRAPPER_INCLUDE_DIR}")
#include_directories("${FFMPEGWRAPPER_INCLUDE_DIR}")
#include_directories("${PROCESSUTILITY_INCLUDE_DIR}")
#include_directories("${SAFEFILESYSTEM_INCLUDE_DIR}")
include_directories("${LIBPQXX_INCLUDE_DIR}")
include_directories("${SPDLOG_INCLUDE_DIR}")
include_directories("${NLOHMANN_INCLUDE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/MMSStorage/src")
include_directories("${PROJECT_SOURCE_DIR}/MMSEngine/src")
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
#if(APPLE)
#include_directories("${PROJECT_SOURCE_DIR}/MMSEngine/src")
#include_directories(
#"${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include"
#)
#include_directories(
#"/usr/local/Cellar/imagemagick/7.0.7-22/include/mageMagick-7")
#else()
## ImageMagick, inside the deploy dir, is a link to the real version:
## ImageMagick -> ImageMagick-7.0.10
#include_directories(
#"${PROJECT_SOURCE_DIR}/../deploy/ImageMagick/include/ImageMagick-7")
#include_directories("${PROJECT_SOURCE_DIR}/../deploy/opencv/include/opencv4")
#endif()

#if(APPLE)
#link_directories("${PROJECT_SOURCE_DIR}/../usr_local/lib")
#endif()
#link_directories("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/lib")
#link_directories("${PROJECT_SOURCE_DIR}/../deploy/libpqxx/lib")

add_library(MMSDelivery SHARED ${SOURCES} ${HEADERS})

#target_link_libraries(MMSEngine aws-cpp-sdk-core)
#target_link_libraries(MMSEngine aws-cpp-sdk-medialive)
#if(APPLE)
#target_link_libraries(MMSEngine FFMpeg)
#target_link_libraries(MMSEngine mysqlcppconn)
#target_link_libraries(MMSEngine jsoncpp)
#target_link_libraries(MMSEngine curl)
#target_link_libraries(MMSEngine Magick++-7.Q16HDRI)
#target_link_libraries(MMSEngine MultiEventsSet)
#endif()

install(TARGETS MMSDelivery DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)
