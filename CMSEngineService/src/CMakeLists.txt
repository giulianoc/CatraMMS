
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
	cmsEngineService.cpp
	CMSEngineProcessor.cpp
	CheckIngestionTimes.cpp
)

SET (HEADERS
	CMSEngineProcessor.h
	CheckIngestionTimes.h
)

include_directories ("${PROJECT_SOURCE_DIR}/../usr_local/include")
include_directories ("${PROJECT_SOURCE_DIR}/../spdlog/include")
include_directories ("${PROJECT_SOURCE_DIR}/../connection-pool")
include_directories ("${PROJECT_SOURCE_DIR}/CMSEngine/src")
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
else()
	include_directories ("/app/5/ThirdPartySoftware/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/include")
endif()

link_directories ("${PROJECT_SOURCE_DIR}/../usr_local/lib")
if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
else()
	link_directories ("/app/5/ThirdPartySoftware/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/lib")
endif()

add_executable(cmsEngineService ${SOURCES} ${HEADERS})

target_link_libraries (cmsEngineService MultiEventsSet)
target_link_libraries (cmsEngineService CMSEngine)
target_link_libraries (cmsEngineService mysqlcppconn)
target_link_libraries (cmsEngineService jsoncpp)

target_link_libraries (cmsEngineService Scheduler)
target_link_libraries (cmsEngineService PThread)
target_link_libraries (cmsEngineService Tools)
target_link_libraries (cmsEngineService ErrMsg)
if(CRYPTO_FOUND)
	if (APPLE)
		target_link_libraries (cmsEngineService ${CRYPTO_LIBRARIES})
	else()
		target_link_libraries (cmsEngineService crypt)
	endif()
endif()
if (ZLIB_FOUND)
	target_link_libraries(cmsEngineService ${ZLIB_LIBRARIES})
endif()
target_link_libraries (cmsEngineService pthread)
