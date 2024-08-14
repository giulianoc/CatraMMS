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

# set(COMMON_SOURCES APICommon.cpp)

set(SOURCES_API
    api.cpp
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
    API_Statistic.cpp)

# SET (SOURCES_UPLOADBINARY UploadBinary.cpp )

set(SOURCES_FFMPEGENCODER
    ffmpegEncoder.cpp
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
    # APICommon.h
    FFMPEGEncoderDaemons.h
    LiveGrid.h
    OverlayTextOnVideo.h
    API.h
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
    UploadBinaryNotUsed.h)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(
  COMMAND ${LSB_RELEASE_EXEC} -is
  OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
  OUTPUT_STRIP_TRAILING_WHITESPACE)

include_directories("${PROJECT_SOURCE_DIR}/FastCGIAPI/src")
include_directories("${PROJECT_SOURCE_DIR}/MMSUtility/src")
include_directories("${PROJECT_SOURCE_DIR}/PostgresHelper/src")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/spdlog/include")
include_directories("${PROJECT_SOURCE_DIR}/MMSEngine/src")
include_directories("${PROJECT_SOURCE_DIR}/FFMpeg/src")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/curlpp/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/nlohmann/include")
include_directories("/usr/include/libxml2")
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
  include_directories("/usr/include/mysql-cppconn/jdbc")
endif()
if(APPLE)
  include_directories(
    "${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include"
  )
endif()

link_directories("${PROJECT_SOURCE_DIR}/../deploy/lib")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/lib")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/lib")
if(APPLE)
  link_directories(
    "${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib"
  )
else()
  # ImageMagick, inside the deploy dir, is a link to the real version:
  # ImageMagick -> ImageMagick-7.0.10
  link_directories("${PROJECT_SOURCE_DIR}/../deploy/ImageMagick/lib")
  link_directories("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib")
  link_directories("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib64")
endif()
# ffmpeg, inside the deploy dir, is a link to the real version: ffmpeg ->
# ffmpeg-4.3.1
link_directories("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg/lib")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg/lib64")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/curlpp/lib")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/curlpp/lib64")
# link_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib64")

# API add_executable(api.fcgi ${SOURCES_API} ${COMMON_SOURCES} ${HEADERS})
add_executable(api.fcgi ${SOURCES_API} ${HEADERS})
target_link_libraries(api.fcgi fastCGIAPI)
target_link_libraries(api.fcgi MMSUtility)
target_link_libraries(api.fcgi MMSEngine)
target_link_libraries(api.fcgi postgresHelper)
target_link_libraries(api.fcgi FFMpeg)
target_link_libraries(api.fcgi mysqlcppconn)
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
target_link_libraries(api.fcgi curlpp)
target_link_libraries(api.fcgi fcgi)
target_link_libraries(api.fcgi Tools)
target_link_libraries(api.fcgi ErrMsg)
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
list(APPEND TARGETS api.fcgi)

# UploadBinary add_executable(uploadBinary.cgi ${SOURCES_UPLOADBINARY}
# ${COMMON_SOURCES} ${HEADERS}) target_link_libraries (uploadBinary.cgi
# MMSEngine) target_link_libraries (uploadBinary.cgi FFMpeg)
# target_link_libraries (uploadBinary.cgi mysqlcppconn) target_link_libraries
# (uploadBinary.cgi jsoncpp) if (APPLE) else() target_link_libraries
# (uploadBinary.cgi Magick++-7.Q16HDRI) endif()
# #target_link_libraries(uploadBinary.cgi fcgi++)
# target_link_libraries(uploadBinary.cgi curl)
# target_link_libraries(uploadBinary.cgi curlpp)
# target_link_libraries(uploadBinary.cgi fcgi) target_link_libraries
# (uploadBinary.cgi Tools) target_link_libraries (uploadBinary.cgi ErrMsg)
# if(CRYPTO_FOUND) if (APPLE) target_link_libraries (uploadBinary.cgi
# ${CRYPTO_LIBRARIES}) else() target_link_libraries (uploadBinary.cgi crypt)
# endif() endif() if (ZLIB_FOUND) target_link_libraries(uploadBinary.cgi
# ${ZLIB_LIBRARIES}) endif() target_link_libraries (uploadBinary.cgi pthread)
# list(APPEND TARGETS uploadBinary.cgi)

# FFMPEGEncoder add_executable(ffmpegEncoder.fcgi ${SOURCES_FFMPEGENCODER}
# ${COMMON_SOURCES} ${HEADERS})
add_executable(ffmpegEncoder.fcgi ${SOURCES_FFMPEGENCODER} ${HEADERS})
target_link_libraries(ffmpegEncoder.fcgi fastCGIAPI)
target_link_libraries(ffmpegEncoder.fcgi MMSUtility)
target_link_libraries(ffmpegEncoder.fcgi MMSEngine)
target_link_libraries(ffmpegEncoder.fcgi postgresHelper)
target_link_libraries(ffmpegEncoder.fcgi FFMpeg)
target_link_libraries(ffmpegEncoder.fcgi mysqlcppconn)
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
# target_link_libraries(ffmpegEncoder.fcgi fcgi++)
target_link_libraries(ffmpegEncoder.fcgi curl)
target_link_libraries(ffmpegEncoder.fcgi curlpp)
target_link_libraries(ffmpegEncoder.fcgi fcgi)
target_link_libraries(ffmpegEncoder.fcgi Tools)
target_link_libraries(ffmpegEncoder.fcgi ErrMsg)
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
list(APPEND TARGETS ffmpegEncoder.fcgi)

install(TARGETS api.fcgi ffmpegEncoder.fcgi DESTINATION bin/cgi)
