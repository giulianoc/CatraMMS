
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
	addWorkspaceToExistingActiveDirectoryUser.cpp
)

SET (HEADERS
)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

include_directories ("${PROJECT_SOURCE_DIR}/../deploy/include")
include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/include")
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
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ImageMagick/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/lib")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/curlpp/lib64")
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/curlpp/lib")
endif()
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib64")
endif()
if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
else()
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib64")
endif()
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg/lib64")

add_executable(addWorkspaceToExistingActiveDirectoryUser ${SOURCES} ${HEADERS})

target_link_libraries (addWorkspaceToExistingActiveDirectoryUser MMSEngine)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser FFMpeg)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser curl)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser curlpp)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser mysqlcppconn)
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser jsoncpp)
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser "${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib/libjsoncpp.a")
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser aom)
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser fdk-aac)
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser x264)
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser x265)
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser srt)
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser opencore-amrwb)
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser opencore-amrnb)
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser mp3lame)
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser xvidcore)
endif()
if (APPLE)
else()
	target_link_libraries (addWorkspaceToExistingActiveDirectoryUser Magick++-7.Q16HDRI)
endif()

#target_link_libraries (registerAdministratorCustomer Scheduler)
#target_link_libraries (registerAdministratorCustomer PThread)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser Tools)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser ErrMsg)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser MultiEventsSet)
if(CRYPTO_FOUND)
	if (APPLE)
		target_link_libraries (addWorkspaceToExistingActiveDirectoryUser ${CRYPTO_LIBRARIES})
	else()
		target_link_libraries (addWorkspaceToExistingActiveDirectoryUser crypt)
	endif()
endif()
if (ZLIB_FOUND)
	target_link_libraries(addWorkspaceToExistingActiveDirectoryUser ${ZLIB_LIBRARIES})
endif()
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser pthread)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser opencv_core)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser opencv_imgcodecs)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser opencv_imgproc)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser opencv_videoio)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser opencv_video)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser opencv_videostab)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser opencv_objdetect)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser opencv_face)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser avcodec)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser avformat)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser avutil)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser swscale)
target_link_libraries (addWorkspaceToExistingActiveDirectoryUser swresample)

install (TARGETS addWorkspaceToExistingActiveDirectoryUser DESTINATION bin)

