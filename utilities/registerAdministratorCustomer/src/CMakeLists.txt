
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

include_directories ("${PROJECT_SOURCE_DIR}/../spdlog/include")
include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
else()
	include_directories ("/app/5/ThirdPartySoftware/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/include")
endif()
include_directories ("${PROJECT_SOURCE_DIR}/../usr_local/include")

link_directories ("${PROJECT_SOURCE_DIR}/../usr_local/lib")
if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
else()
	link_directories ("/app/5/ThirdPartySoftware/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/lib")
	link_directories ("/home/giuliano/catramms/opencv/lib")
endif()

add_executable(registerAdministratorCustomer ${SOURCES} ${HEADERS})

target_link_libraries (registerAdministratorCustomer MMSEngine)
target_link_libraries (registerAdministratorCustomer FFMpeg)
target_link_libraries (registerAdministratorCustomer curl)
target_link_libraries (registerAdministratorCustomer curlpp)
target_link_libraries (registerAdministratorCustomer mysqlcppconn)
target_link_libraries (registerAdministratorCustomer jsoncpp)
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

install (TARGETS registerAdministratorCustomer DESTINATION bin)

