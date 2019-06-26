
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
	registerAdministratorCustomer.cpp
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
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ImageMagick-7.0.8-49/lib")
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
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib64")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg-4.1.3/lib")
endif()

add_executable(registerAdministratorCustomer ${SOURCES} ${HEADERS})

target_link_libraries (registerAdministratorCustomer MMSEngine)
target_link_libraries (registerAdministratorCustomer FFMpeg)
target_link_libraries (registerAdministratorCustomer curl)
target_link_libraries (registerAdministratorCustomer curlpp)
target_link_libraries (registerAdministratorCustomer mysqlcppconn)
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	target_link_libraries (registerAdministratorCustomer jsoncpp)
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
	target_link_libraries (registerAdministratorCustomer "${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib/libjsoncpp.a")
endif()
if (APPLE)
else()
	target_link_libraries (registerAdministratorCustomer Magick++-7.Q16HDRI)
endif()

#target_link_libraries (registerAdministratorCustomer Scheduler)
#target_link_libraries (registerAdministratorCustomer PThread)
target_link_libraries (registerAdministratorCustomer Tools)
target_link_libraries (registerAdministratorCustomer ErrMsg)
target_link_libraries (registerAdministratorCustomer MultiEventsSet)
if(CRYPTO_FOUND)
	if (APPLE)
		target_link_libraries (registerAdministratorCustomer ${CRYPTO_LIBRARIES})
	else()
		target_link_libraries (registerAdministratorCustomer crypt)
	endif()
endif()
if (ZLIB_FOUND)
	target_link_libraries(registerAdministratorCustomer ${ZLIB_LIBRARIES})
endif()
target_link_libraries (registerAdministratorCustomer pthread)
target_link_libraries (registerAdministratorCustomer opencv_core)
target_link_libraries (registerAdministratorCustomer opencv_imgcodecs)
target_link_libraries (registerAdministratorCustomer opencv_imgproc)
target_link_libraries (registerAdministratorCustomer opencv_videoio)
target_link_libraries (registerAdministratorCustomer opencv_video)
target_link_libraries (registerAdministratorCustomer opencv_videostab)
target_link_libraries (registerAdministratorCustomer opencv_objdetect)
target_link_libraries (registerAdministratorCustomer opencv_face)

install (TARGETS registerAdministratorCustomer DESTINATION bin)

