
# Copyright (C) Giuliano Catrambone (giuliano.catrambone@catrasoftware.it)

# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License 
# as published by the Free Software Foundation; either 
# version 2 of the License, or (at your option) any later 
# version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# Commercial use other than under the terms of the GNU General Public
# License is allowed only after express negotiation of conditions
# with the authors.

SET (SOURCES
	sendEmail.cpp
)

SET (HEADERS
)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

include_directories ("${PROJECT_SOURCE_DIR}/CatraLibraries/CurlWrapper/src")
include_directories ("${PROJECT_SOURCE_DIR}/CatraLibraries/JSONUtils/src")
include_directories ("${PROJECT_SOURCE_DIR}/CatraLibraries/Encrypt/src")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/spdlog/include")
include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/nlohmann/include")
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
else()
endif()

link_directories ("${PROJECT_SOURCE_DIR}/../deploy/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/lib")
if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
else()
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ImageMagick/lib")
endif()
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg/lib64")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib64")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib64")
endif()

add_executable(sendEmail ${SOURCES} ${HEADERS})

target_link_libraries (sendEmail CurlWrapper)
target_link_libraries (sendEmail JSONUtils)
target_link_libraries (sendEmail Datetime)
target_link_libraries (sendEmail MMSEngine)
target_link_libraries (sendEmail FFMpegWrapper)
target_link_libraries (sendEmail curl)
target_link_libraries (sendEmail mysqlcppconn)
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	target_link_libraries (sendEmail jsoncpp)
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
  #target_link_libraries (sendEmail "${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib/libjsoncpp.a")
	target_link_libraries (sendEmail aom)
	target_link_libraries (sendEmail fdk-aac)
	target_link_libraries (sendEmail x264)
	target_link_libraries (sendEmail x265)
	target_link_libraries (sendEmail srt)
	target_link_libraries (sendEmail opencore-amrwb)
	target_link_libraries (sendEmail opencore-amrnb)
	target_link_libraries (sendEmail mp3lame)
	target_link_libraries (sendEmail xvidcore)
endif()
if (APPLE) 
else()  
        target_link_libraries (sendEmail Magick++-7.Q16HDRI)
endif() 


#target_link_libraries (sendEmail Scheduler)
#target_link_libraries (sendEmail PThread)
target_link_libraries (sendEmail Encrypt)
target_link_libraries (sendEmail MultiEventsSet)
if(CRYPTO_FOUND)
	if (APPLE)
		target_link_libraries (sendEmail ${CRYPTO_LIBRARIES})
	else()
		target_link_libraries (sendEmail crypt)
	endif()
endif()
if (ZLIB_FOUND)
	target_link_libraries(sendEmail ${ZLIB_LIBRARIES})
endif()
target_link_libraries (sendEmail pthread)
target_link_libraries (sendEmail opencv_core)
target_link_libraries (sendEmail opencv_imgcodecs)
target_link_libraries (sendEmail opencv_imgproc)
target_link_libraries (sendEmail opencv_videoio)
target_link_libraries (sendEmail opencv_video)
target_link_libraries (sendEmail opencv_videostab)
target_link_libraries (sendEmail opencv_objdetect)
target_link_libraries (sendEmail opencv_face)
target_link_libraries (sendEmail avcodec)
target_link_libraries (sendEmail avformat)
target_link_libraries (sendEmail avutil)
target_link_libraries (sendEmail swscale)
target_link_libraries (sendEmail swresample)

install (TARGETS sendEmail DESTINATION bin)

