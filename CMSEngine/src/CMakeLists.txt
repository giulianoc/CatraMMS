
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
	Customer.cpp
	CMSStorage.cpp
	CMSEngineDBFacade.cpp
	CMSEngine.cpp
	ActiveEncodingsManager.cpp
	EncoderVideoAudioProxy.cpp
)
SET (HEADERS
	Customer.h
	CMSStorage.h
	CMSEngineDBFacade.h
	CMSEngine.h
	ActiveEncodingsManager.h
	EncoderVideoAudioProxy.h
)

include_directories ("${PROJECT_SOURCE_DIR}/../usr_local/include")
include_directories ("${PROJECT_SOURCE_DIR}/../spdlog/include")
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
else()
	include_directories ("/app/5/ThirdPartySoftware/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/include")
endif()

if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../usr_local/lib")
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
endif()

add_library (CMSEngine SHARED ${SOURCES} ${HEADERS})

if (APPLE)
	target_link_libraries (CMSEngine ErrMsg)
	target_link_libraries (CMSEngine Tools)
	target_link_libraries (CMSEngine mysqlcppconn)
	target_link_libraries (CMSEngine jsoncpp)
endif()
