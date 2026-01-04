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

add_definitions(-DNO_FCGI_DEFINES)

set(SOURCES
        apiMain.cpp
        API.cpp
        API_DeliveryAuthorization.cpp
        API_Encoding.cpp
        API_Ingestion.cpp
        API_MediaItem.cpp
        API_WorkflowLibrary.cpp
        API_UserWorkspace.cpp
        API_Invoice.cpp
        API_Configuration.cpp
        API_EncodersPool.cpp
        API_Statistic.cpp
)

set(HEADERS
        API.h
)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(
  COMMAND ${LSB_RELEASE_EXEC} -is
  OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
  OUTPUT_STRIP_TRAILING_WHITESPACE)

include_directories("${STRINGUTILS_INCLUDE_DIR}")
include_directories("${GETCPUUSAGE_INCLUDE_DIR}")
include_directories("${DATETIME_INCLUDE_DIR}")
include_directories("${ENCRYPT_INCLUDE_DIR}")
include_directories("${SYSTEM_INCLUDE_DIR}")
include_directories("${DBCONN_INCLUDE_DIR}")
include_directories("${CURLWRAPPER_INCLUDE_DIR}")
include_directories("${CONVERT_INCLUDE_DIR}")
include_directories("${POSTGRESHELPER_INCLUDE_DIR}")
include_directories("${PROCESSUTILITY_INCLUDE_DIR}")
include_directories("${BANDWIDTHUSAGETHREAD_INCLUDE_DIR}")
include_directories("${JSONUTILS_INCLUDE_DIR}")
include_directories("${FASTCGIAPI_INCLUDE_DIR}")
include_directories("${FFMPEGWRAPPER_INCLUDE_DIR}")
include_directories("${LDAPWRAPPER_INCLUDE_DIR}")
include_directories("${LIBPQXX_INCLUDE_DIR}")
include_directories("${SPDLOG_INCLUDE_DIR}")
include_directories("${NLOHMANN_INCLUDE_DIR}")
include_directories("${SAFEFILESYSTEM_INCLUDE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/MMSStorage/src")
include_directories("${PROJECT_SOURCE_DIR}/MMSDelivery/src")
include_directories("${PROJECT_SOURCE_DIR}/MMSEngine/src")
include_directories("/usr/include/libxml2")
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
  include_directories("/usr/include/mysql-cppconn/jdbc")
endif()
if(APPLE)
  include_directories("${FCGI_INCLUDE_DIR}")
  include_directories("${OPENSSL_INCLUDE_DIR}")
endif()

if(APPLE)
  link_directories("${LIBPQXX_LIB_DIR}")
  link_directories("${LIBPQ_LIB_DIR}")
  link_directories("${FCGI_LIB_DIR}")
  link_directories("${OPENCV_LIB_DIR}")
  link_directories("${FFMPEG_LIB_DIR}")
  link_directories("${OPENSSL_LIB_DIR}")
endif()

link_directories("${PROJECT_SOURCE_DIR}/../deploy/lib")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/lib")
link_directories("${IMAGEMAGICK_LIB_DIR}")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib64")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg/lib")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/libpqxx/lib")
# link_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib64")

add_executable(api.fcgi ${SOURCES} ${HEADERS})

target_link_libraries(api.fcgi StringUtils)
target_link_libraries(api.fcgi Encrypt)
target_link_libraries(api.fcgi Datetime)
target_link_libraries(api.fcgi FastCGIAPI)
target_link_libraries(api.fcgi MMSDelivery)
target_link_libraries(api.fcgi BandwidthUsageThread)
target_link_libraries(api.fcgi MMSStorage)
target_link_libraries(api.fcgi MMSEngine)
target_link_libraries(api.fcgi System)
target_link_libraries(api.fcgi GetCpuUsage)
target_link_libraries(api.fcgi PostgresHelper)
target_link_libraries(api.fcgi SafeFileSystem)
target_link_libraries(api.fcgi ProcessUtility)
target_link_libraries(api.fcgi CurlWrapper)
target_link_libraries(api.fcgi Convert)
target_link_libraries(api.fcgi JSONUtils)
target_link_libraries(api.fcgi FFMpegWrapper)
target_link_libraries(api.fcgi FFMpegEngine)
if(DBCONNECTION_MYSQL)
  target_link_libraries(api.fcgi PRIVATE mysqlcppconn)
endif()
target_link_libraries(api.fcgi pqxx)
target_link_libraries(api.fcgi pq)
target_link_libraries(api.fcgi xml2)
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
  target_link_libraries(api.fcgi jsoncpp)
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
  # target_link_libraries (api.fcgi
  # ${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib/libjsoncpp.a)
  target_link_libraries(api.fcgi aom)
  target_link_libraries(api.fcgi opencore-amrwb)
  target_link_libraries(api.fcgi opencore-amrnb)
  target_link_libraries(api.fcgi mp3lame)
  target_link_libraries(api.fcgi xvidcore)
  target_link_libraries(api.fcgi fdk-aac)
  target_link_libraries(api.fcgi x264)
  target_link_libraries(api.fcgi x265)
  target_link_libraries(api.fcgi srt)
endif()
if(APPLE)

else()
  target_link_libraries(api.fcgi Magick++-7.Q16HDRI)
endif()
# target_link_libraries(api.fcgi fcgi++)
target_link_libraries(api.fcgi curl)
target_link_libraries(api.fcgi fcgi)
target_link_libraries(api.fcgi MultiEventsSet)
if(CRYPTO_FOUND)
  if(APPLE)
    target_link_libraries(api.fcgi ${CRYPTO_LIBRARIES})
  else()
    target_link_libraries(api.fcgi crypt)
  endif()
endif()
target_link_libraries(api.fcgi crypto)
if(ZLIB_FOUND)
  target_link_libraries(api.fcgi ${ZLIB_LIBRARIES})
endif()
target_link_libraries(api.fcgi LdapWrapper)
target_link_libraries(api.fcgi pthread)
target_link_libraries(api.fcgi opencv_core)
target_link_libraries(api.fcgi opencv_imgcodecs)
target_link_libraries(api.fcgi opencv_imgproc)
target_link_libraries(api.fcgi opencv_videoio)
target_link_libraries(api.fcgi opencv_video)
target_link_libraries(api.fcgi opencv_videostab)
target_link_libraries(api.fcgi opencv_objdetect)
target_link_libraries(api.fcgi opencv_face)
target_link_libraries(api.fcgi avcodec)
target_link_libraries(api.fcgi avformat)
target_link_libraries(api.fcgi avutil)
target_link_libraries(api.fcgi swscale)
target_link_libraries(api.fcgi swresample)
#list(APPEND TARGETS api.fcgi)

install(TARGETS api.fcgi DESTINATION bin/cgi)
