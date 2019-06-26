
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

ADD_DEFINITIONS(-DNO_FCGI_DEFINES)

SET (COMMON_SOURCES
	APICommon.cpp
)

SET (SOURCES_API
	API.cpp
	API_Encoding.cpp
	API_Ingestion.cpp
	API_UserWorkspace.cpp
	API_Configuration.cpp
)

#SET (SOURCES_UPLOADBINARY
#	UploadBinary.cpp
#)

SET (SOURCES_FFMPEGENCODER
	FFMPEGEncoder.cpp
)

SET (HEADERS
	APICommon.h
)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

include_directories ("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/include")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/include")
include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
include_directories ("${PROJECT_SOURCE_DIR}/FFMpeg/src")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/curlpp/include")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/include")
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	include_directories ("/usr/include/mysql-cppconn/jdbc")
endif()
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
endif()

link_directories ("${PROJECT_SOURCE_DIR}/../deploy/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/lib")
if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
else()
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ImageMagick-7.0.8-49/lib")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib64")
endif()
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg-4.1.3/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/curlpp/lib64")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib64")

#API
    add_executable(api.fcgi ${SOURCES_API} ${COMMON_SOURCES} ${HEADERS})
    target_link_libraries (api.fcgi MMSEngine)
    target_link_libraries (api.fcgi FFMpeg)
    target_link_libraries (api.fcgi mysqlcppconn)
    target_link_libraries (api.fcgi jsoncpp)
    if (APPLE)
    else()
	    target_link_libraries (api.fcgi Magick++-7.Q16HDRI)
    endif()
	#target_link_libraries(api.fcgi fcgi++)
    target_link_libraries(api.fcgi curl)
    target_link_libraries(api.fcgi curlpp)
    target_link_libraries(api.fcgi fcgi)
    target_link_libraries (api.fcgi Tools)
    target_link_libraries (api.fcgi ErrMsg)
    target_link_libraries (api.fcgi MultiEventsSet)
    if(CRYPTO_FOUND)
	    if (APPLE)
		    target_link_libraries (api.fcgi ${CRYPTO_LIBRARIES})
	    else()
		target_link_libraries (api.fcgi crypt)
	    endif()
    endif()
    if (ZLIB_FOUND)
	    target_link_libraries(api.fcgi ${ZLIB_LIBRARIES})
    endif()
    target_link_libraries (api.fcgi pthread)
	target_link_libraries (api.fcgi opencv_core)
	target_link_libraries (api.fcgi opencv_imgcodecs)
	target_link_libraries (api.fcgi opencv_imgproc)
	target_link_libraries (api.fcgi opencv_videoio)
	target_link_libraries (api.fcgi opencv_video)
	target_link_libraries (api.fcgi opencv_videostab)
	target_link_libraries (api.fcgi opencv_objdetect)
	target_link_libraries (api.fcgi opencv_face)
    list(APPEND TARGETS api.fcgi)

#UploadBinary
#    add_executable(uploadBinary.cgi ${SOURCES_UPLOADBINARY} ${COMMON_SOURCES} ${HEADERS})
#    target_link_libraries (uploadBinary.cgi MMSEngine)
#    target_link_libraries (uploadBinary.cgi FFMpeg)
#    target_link_libraries (uploadBinary.cgi mysqlcppconn)
#    target_link_libraries (uploadBinary.cgi jsoncpp)
#    if (APPLE)
#    else()
#	    target_link_libraries (uploadBinary.cgi Magick++-7.Q16HDRI)
#    endif()
#	#target_link_libraries(uploadBinary.cgi fcgi++)
#    target_link_libraries(uploadBinary.cgi curl)
#    target_link_libraries(uploadBinary.cgi curlpp)
#    target_link_libraries(uploadBinary.cgi fcgi)
#    target_link_libraries (uploadBinary.cgi Tools)
#target_link_libraries (uploadBinary.cgi ErrMsg)
#    if(CRYPTO_FOUND)
#	    if (APPLE)
#		    target_link_libraries (uploadBinary.cgi ${CRYPTO_LIBRARIES})
#	    else()
#		target_link_libraries (uploadBinary.cgi crypt)
#	    endif()
#    endif()
#    if (ZLIB_FOUND)
#	    target_link_libraries(uploadBinary.cgi ${ZLIB_LIBRARIES})
#    endif()
#    target_link_libraries (uploadBinary.cgi pthread)
#    list(APPEND TARGETS uploadBinary.cgi)

#FFMPEGEncoder
    add_executable(ffmpegEncoder.fcgi ${SOURCES_FFMPEGENCODER} ${COMMON_SOURCES} ${HEADERS})
    target_link_libraries (ffmpegEncoder.fcgi MMSEngine)
    target_link_libraries (ffmpegEncoder.fcgi FFMpeg)
    target_link_libraries (ffmpegEncoder.fcgi mysqlcppconn)
    target_link_libraries (ffmpegEncoder.fcgi jsoncpp)
    if (APPLE)
    else()
	    target_link_libraries (ffmpegEncoder.fcgi Magick++-7.Q16HDRI)
    endif()
	#target_link_libraries(ffmpegEncoder.fcgi fcgi++)
    target_link_libraries(ffmpegEncoder.fcgi curl)
    target_link_libraries(ffmpegEncoder.fcgi curlpp)
    target_link_libraries(ffmpegEncoder.fcgi fcgi)
    target_link_libraries (ffmpegEncoder.fcgi Tools)
    target_link_libraries (ffmpegEncoder.fcgi ErrMsg)
    target_link_libraries (ffmpegEncoder.fcgi MultiEventsSet)
    if(CRYPTO_FOUND)
	    if (APPLE)
		    target_link_libraries (ffmpegEncoder.fcgi ${CRYPTO_LIBRARIES})
	    else()
		target_link_libraries (ffmpegEncoder.fcgi crypt)
	    endif()
    endif()
    if (ZLIB_FOUND)
	    target_link_libraries(ffmpegEncoder.fcgi ${ZLIB_LIBRARIES})
    endif()
    target_link_libraries (ffmpegEncoder.fcgi pthread)
	target_link_libraries (ffmpegEncoder.fcgi opencv_core)
	target_link_libraries (ffmpegEncoder.fcgi opencv_imgcodecs)
	target_link_libraries (ffmpegEncoder.fcgi opencv_imgproc)
	target_link_libraries (ffmpegEncoder.fcgi opencv_videoio)
	target_link_libraries (ffmpegEncoder.fcgi opencv_video)
	target_link_libraries (ffmpegEncoder.fcgi opencv_videostab)
	target_link_libraries (ffmpegEncoder.fcgi opencv_objdetect)
	target_link_libraries (ffmpegEncoder.fcgi opencv_face)
	target_link_libraries (ffmpegEncoder.fcgi avcodec)
    list(APPEND TARGETS ffmpegEncoder.fcgi)


install (TARGETS api.fcgi ffmpegEncoder.fcgi DESTINATION bin/cgi)

