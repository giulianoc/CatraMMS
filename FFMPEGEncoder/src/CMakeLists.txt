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
    ffmpegEncoderMain.cpp
    FFMPEGEncoder.cpp
    FFMPEGEncoderDaemons.cpp
    FFMPEGEncoderBase.cpp
    FFMPEGEncoderTask.cpp
    EncodeContent.cpp
    OverlayImageOnVideo.cpp
    OverlayTextOnVideo.cpp
    GenerateFrames.cpp
    SlideShow.cpp
    VideoSpeed.cpp
    AddSilentAudio.cpp
    PictureInPicture.cpp
    IntroOutroOverlay.cpp
    CutFrameAccurate.cpp
    LiveRecorder.cpp
    LiveRecorderDaemons.cpp
    LiveProxy.cpp
    LiveGrid.cpp)

set(HEADERS
    AddSilentAudio.h
    FFMPEGEncoderBase.h
    IntroOutroOverlay.h
    OverlayImageOnVideo.h
    VideoSpeed.h
    FFMPEGEncoderDaemons.h
    LiveGrid.h
    OverlayTextOnVideo.h
    FFMPEGEncoder.h
    LiveProxy.h
    PictureInPicture.h
    CutFrameAccurate.h
    FFMPEGEncoderTask.h
    LiveRecorderDaemons.h
    SlideShow.h
    EncodeContent.h
    GenerateFrames.h
    LiveRecorder.h
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
include_directories("${JSONUTILS_INCLUDE_DIR}")
include_directories("${FASTCGIAPI_INCLUDE_DIR}")
include_directories("${FFMPEGENGINE_INCLUDE_DIR}")
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
  #include_directories("${OPENSSL_INCLUDE_DIR}")
endif()

if(APPLE)
  link_directories("${LIBPQXX_LIB_DIR}")
  link_directories("${LIBPQ_LIB_DIR}")
  link_directories("${FCGI_LIB_DIR}")
  link_directories("${OPENCV_LIB_DIR}")
  #link_directories("${FFMPEGENGINE_LIB_DIR}")
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

# FFMPEGEncoder add_executable(ffmpegEncoder.fcgi ${SOURCES_FFMPEGENCODER}
# ${COMMON_SOURCES} ${HEADERS})
add_executable(ffmpegEncoder.fcgi ${SOURCES} ${HEADERS})
target_link_libraries(ffmpegEncoder.fcgi StringUtils)
target_link_libraries(ffmpegEncoder.fcgi Encrypt)
target_link_libraries(ffmpegEncoder.fcgi System)
target_link_libraries(ffmpegEncoder.fcgi GetCpuUsage)
target_link_libraries(ffmpegEncoder.fcgi Datetime)
target_link_libraries(ffmpegEncoder.fcgi FastCGIAPI)
target_link_libraries(ffmpegEncoder.fcgi MMSStorage)
target_link_libraries(ffmpegEncoder.fcgi MMSEngine)
target_link_libraries(ffmpegEncoder.fcgi PostgresHelper)
target_link_libraries(ffmpegEncoder.fcgi SafeFileSystem)
target_link_libraries(ffmpegEncoder.fcgi ProcessUtility)
target_link_libraries(ffmpegEncoder.fcgi CurlWrapper)
target_link_libraries(ffmpegEncoder.fcgi Convert)
target_link_libraries(ffmpegEncoder.fcgi JSONUtils)
target_link_libraries(ffmpegEncoder.fcgi FFMpegWrapper)
target_link_libraries(ffmpegEncoder.fcgi FFMpegEngine)
if(DBCONNECTION_MYSQL)
  target_link_libraries(api.fcgi PRIVATE mysqlcppconn)
endif()
target_link_libraries(ffmpegEncoder.fcgi pqxx)
target_link_libraries(ffmpegEncoder.fcgi pq)
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
  target_link_libraries(ffmpegEncoder.fcgi jsoncpp)
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
  # target_link_libraries (ffmpegEncoder.fcgi
  # ${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib/libjsoncpp.a)
  target_link_libraries(ffmpegEncoder.fcgi aom)
  target_link_libraries(ffmpegEncoder.fcgi opencore-amrwb)
  target_link_libraries(ffmpegEncoder.fcgi opencore-amrnb)
  target_link_libraries(ffmpegEncoder.fcgi mp3lame)
  target_link_libraries(ffmpegEncoder.fcgi xvidcore)
  target_link_libraries(ffmpegEncoder.fcgi fdk-aac)
  target_link_libraries(ffmpegEncoder.fcgi x264)
  target_link_libraries(ffmpegEncoder.fcgi x265)
  target_link_libraries(ffmpegEncoder.fcgi srt)
endif()
if(APPLE)

else()
  target_link_libraries(ffmpegEncoder.fcgi Magick++-7.Q16HDRI)
endif()
target_link_libraries(ffmpegEncoder.fcgi curl)
target_link_libraries(ffmpegEncoder.fcgi fcgi)
target_link_libraries(ffmpegEncoder.fcgi MultiEventsSet)
if(CRYPTO_FOUND)
  if(APPLE)
    target_link_libraries(ffmpegEncoder.fcgi ${CRYPTO_LIBRARIES})
  else()
    target_link_libraries(ffmpegEncoder.fcgi crypt)
  endif()
endif()
if(ZLIB_FOUND)
  target_link_libraries(ffmpegEncoder.fcgi ${ZLIB_LIBRARIES})
endif()
target_link_libraries(ffmpegEncoder.fcgi pthread)
target_link_libraries(ffmpegEncoder.fcgi opencv_core)
target_link_libraries(ffmpegEncoder.fcgi opencv_imgcodecs)
target_link_libraries(ffmpegEncoder.fcgi opencv_imgproc)
target_link_libraries(ffmpegEncoder.fcgi opencv_videoio)
target_link_libraries(ffmpegEncoder.fcgi opencv_video)
target_link_libraries(ffmpegEncoder.fcgi opencv_videostab)
target_link_libraries(ffmpegEncoder.fcgi opencv_objdetect)
target_link_libraries(ffmpegEncoder.fcgi opencv_face)
target_link_libraries(ffmpegEncoder.fcgi avcodec)
target_link_libraries(ffmpegEncoder.fcgi avformat)
target_link_libraries(ffmpegEncoder.fcgi avutil)
target_link_libraries(ffmpegEncoder.fcgi swscale)
target_link_libraries(ffmpegEncoder.fcgi swresample)
#list(APPEND TARGETS ffmpegEncoder.fcgi)

install(TARGETS ffmpegEncoder.fcgi DESTINATION bin/cgi)
