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
    FFMpegFilters.cpp
    FFMpegEncodingParameters.cpp
    FFMpeg.cpp
    FFMpeg_Monitoring.cpp
    FFMpeg_EncodeContent.cpp
    FFMpeg_Overlay.cpp
    FFMpeg_IntroOutroOverlay.cpp
    FFMpeg_VideoSpeed.cpp
    FFMpeg_PictureInPicture.cpp
    FFMpeg_SilentAudio.cpp
    FFMpeg_MediaInfo.cpp
    FFMpeg_ConcatCutMuxSplit.cpp
    FFMpeg_Frames.cpp
    FFMpeg_SlideShow.cpp
    FFMpeg_LiveRecorder.cpp
    FFMpeg_LiveProxy.cpp
    FFMpeg_LiveGrid.cpp
    FFMpeg_ChangeFileFormat.cpp
    FFMpeg_StreamingToFile.cpp
    FFMpeg_Incrontab.cpp
    FFMpeg_Youtube.cpp)
set(HEADERS FFMpegFilters.h FFMpegEncodingParameters.h FFMpeg.h)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(
  COMMAND ${LSB_RELEASE_EXEC} -is
  OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
  OUTPUT_STRIP_TRAILING_WHITESPACE)
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
  include_directories("/usr/include/mysql-cppconn/jdbc")
endif()

include_directories("${PROJECT_SOURCE_DIR}/MMSUtility/src")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/curlpp/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/spdlog/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/nlohmann/include")
if(APPLE)
  include_directories(
    "${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include"
  )
  include_directories(
    "/usr/local/Cellar/imagemagick/7.0.7-22/include/ImageMagick-7")
endif()

if(APPLE)
  link_directories("${PROJECT_SOURCE_DIR}/../usr_local/lib")
  link_directories(
    "${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib"
  )
endif()

add_library(FFMpeg SHARED ${SOURCES} ${HEADERS})

if(APPLE)
  target_link_libraries(FFMpeg ErrMsg)
  target_link_libraries(FFMpeg Tools)
  target_link_libraries(FFMpeg mysqlcppconn)
  target_link_libraries(FFMpeg jsoncpp)
  target_link_libraries(FFMpeg Magick++-7.Q16HDRI)
endif()

install(TARGETS FFMpeg DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)
