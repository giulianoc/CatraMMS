
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
	lockTest.cpp
)

SET (HEADERS
)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

include_directories ("${PROJECT_SOURCE_DIR}/../deploy/spdlog/include")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/lib")
include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/include")
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	include_directories ("/usr/include/mysql-cppconn/jdbc")
endif()
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
else()
endif()

link_directories ("${PROJECT_SOURCE_DIR}/../deploy/lib")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib64")
endif()
if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
else()
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ImageMagick/lib")
endif()
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg/lib64")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib64")

add_executable(lockTest ${SOURCES} ${HEADERS})

target_link_libraries (lockTest MMSEngine)
target_link_libraries (lockTest FFMpeg)
target_link_libraries (lockTest curl)
target_link_libraries (lockTest mysqlcppconn)
target_link_libraries (lockTest pqxx)
target_link_libraries (lockTest pq)
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	target_link_libraries (lockTest jsoncpp)
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
	target_link_libraries (lockTest "${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib/libjsoncpp.a")
	target_link_libraries (lockTest aom)
	target_link_libraries (lockTest fdk-aac)
	target_link_libraries (lockTest x264)
	target_link_libraries (lockTest x265)
	target_link_libraries (lockTest srt)
	target_link_libraries (lockTest opencore-amrwb)
	target_link_libraries (lockTest opencore-amrnb)
	target_link_libraries (lockTest mp3lame)
	target_link_libraries (lockTest xvidcore)
endif()
if (APPLE) 
else()  
        target_link_libraries (lockTest Magick++-7.Q16HDRI)
endif() 


target_link_libraries (lockTest MultiEventsSet)
if(CRYPTO_FOUND)
	if (APPLE)
		target_link_libraries (lockTest ${CRYPTO_LIBRARIES})
	else()
		target_link_libraries (lockTest crypt)
	endif()
endif()
if (ZLIB_FOUND)
	target_link_libraries(lockTest ${ZLIB_LIBRARIES})
endif()
target_link_libraries (lockTest pthread)
target_link_libraries (lockTest opencv_core)
target_link_libraries (lockTest opencv_imgcodecs)
target_link_libraries (lockTest opencv_imgproc)
target_link_libraries (lockTest opencv_videoio)
target_link_libraries (lockTest opencv_video)
target_link_libraries (lockTest opencv_videostab)
target_link_libraries (lockTest opencv_objdetect)
target_link_libraries (lockTest opencv_face)
target_link_libraries (lockTest avcodec)
target_link_libraries (lockTest avformat)
target_link_libraries (lockTest avutil)
target_link_libraries (lockTest swscale)
target_link_libraries (lockTest swresample)

install (TARGETS lockTest DESTINATION bin)

