
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
	Workspace.cpp
	MMSStorage.cpp
	MMSEngineDBFacade.cpp
	Validator.cpp
	ActiveEncodingsManager.cpp
	EncoderVideoAudioProxy.cpp
)
SET (HEADERS
	Workspace.h
	MMSStorage.h
	MySQLConnection.h
	MMSEngineDBFacade.h
	Validator.h
	ActiveEncodingsManager.h
	EncoderVideoAudioProxy.h
)

include_directories ("${PROJECT_SOURCE_DIR}/../spdlog/include")
include_directories ("${PROJECT_SOURCE_DIR}/FFMpeg/src")
include_directories ("${PROJECT_SOURCE_DIR}/../usr_local/include")
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
	include_directories ("/usr/local/Cellar/imagemagick/7.0.7-22/include/ImageMagick-7")
else()
	include_directories ("/app/5/ThirdPartySoftware/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/include")
	include_directories ("${PROJECT_SOURCE_DIR}/../usr_local/include/ImageMagick-7")
endif()

if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../usr_local/lib")
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
endif()

add_library (MMSEngine SHARED ${SOURCES} ${HEADERS})

if (APPLE)
	target_link_libraries (MMSEngine FFMpeg)
	target_link_libraries (MMSEngine ErrMsg)
	target_link_libraries (MMSEngine Tools)
	target_link_libraries (MMSEngine mysqlcppconn)
	target_link_libraries (MMSEngine jsoncpp)
	target_link_libraries (MMSEngine curl)
	target_link_libraries (MMSEngine curlpp)
        target_link_libraries (MMSEngine Magick++-7.Q16HDRI)
endif()

install (TARGETS MMSEngine DESTINATION lib)
install (FILES ${HEADERS} DESTINATION include)

