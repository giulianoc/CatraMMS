
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
	awsSigner.cpp
)

SET (HEADERS
)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

include_directories ("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/include")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/include")
include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/include")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/curlpp/include")
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
else()
endif()

link_directories ("${PROJECT_SOURCE_DIR}/../deploy/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/lib")
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
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/curlpp/lib64")
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/curlpp/lib")
endif()
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib64")
endif()

add_executable(awsSigner ${SOURCES} ${HEADERS})


target_link_libraries (awsSigner MMSEngine)
target_link_libraries (awsSigner FFMpeg)
target_link_libraries (awsSigner MMSUtility)
target_link_libraries (awsSigner curl)
target_link_libraries (awsSigner curlpp)
target_link_libraries (awsSigner mysqlcppconn)
target_link_libraries (awsSigner pqxx)
target_link_libraries (awsSigner pq)
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	target_link_libraries (awsSigner jsoncpp)
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
	target_link_libraries (awsSigner "${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib/libjsoncpp.a")
	target_link_libraries (awsSigner avcodec)
	target_link_libraries (awsSigner avformat)
	target_link_libraries (awsSigner avutil)
	target_link_libraries (awsSigner swscale)
	target_link_libraries (awsSigner aom)
	target_link_libraries (awsSigner fdk-aac)
	target_link_libraries (awsSigner x264)
	target_link_libraries (awsSigner x265)
	target_link_libraries (awsSigner srt)
	target_link_libraries (awsSigner swresample)
	target_link_libraries (awsSigner opencore-amrwb)
	target_link_libraries (awsSigner opencore-amrnb)
	target_link_libraries (awsSigner mp3lame)
	target_link_libraries (awsSigner xvidcore)
endif()
#if (APPLE) 
#else()  
	target_link_libraries (awsSigner Magick++-7.Q16HDRI)
#	target_link_libraries (awsSigner MagickCore-7.Q16HDRI)
#	target_link_libraries (awsSigner MagickWand-7.Q16HDRI)
#endif() 
#
target_link_libraries (awsSigner Scheduler)
target_link_libraries (awsSigner PThread)
target_link_libraries (awsSigner Tools)
target_link_libraries (awsSigner ErrMsg)
target_link_libraries (awsSigner MultiEventsSet)
if(CRYPTO_FOUND)
	if (APPLE)
		target_link_libraries (awsSigner ${CRYPTO_LIBRARIES})
	else()
		target_link_libraries (awsSigner crypt)
	endif()
endif()
if (ZLIB_FOUND)
	target_link_libraries(awsSigner ${ZLIB_LIBRARIES})
endif()
target_link_libraries (awsSigner pthread)
target_link_libraries (awsSigner crypto)
target_link_libraries (awsSigner opencv_core)
target_link_libraries (awsSigner opencv_imgcodecs)
target_link_libraries (awsSigner opencv_imgproc)
target_link_libraries (awsSigner opencv_videoio)
target_link_libraries (awsSigner opencv_video)
target_link_libraries (awsSigner opencv_videostab)
target_link_libraries (awsSigner opencv_objdetect)
target_link_libraries (awsSigner opencv_face)
target_link_libraries (awsSigner ldap)
#target_link_libraries (awsSigner avcodec)
#target_link_libraries (awsSigner avformat)
#target_link_libraries (awsSigner avutil)
#target_link_libraries (awsSigner swscale)
#target_link_libraries (awsSigner swresample)
#target_link_libraries (awsSigner crypto)
#
install (TARGETS awsSigner DESTINATION bin)

