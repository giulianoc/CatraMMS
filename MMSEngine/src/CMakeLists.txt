
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
	MMSEngineDBFacade_Configuration.cpp
	MMSEngineDBFacade_CreateTables.cpp
	MMSEngineDBFacade_EncodingJobs.cpp
	MMSEngineDBFacade_EncodingProfiles.cpp
	MMSEngineDBFacade_IngestionJobs.cpp
	MMSEngineDBFacade_Lock.cpp
	MMSEngineDBFacade_MediaItems.cpp
	MMSEngineDBFacade_UserWorkspace.cpp
	UpdaterEncoderJob.cpp
	UpdaterIngestionJob.cpp
	PersistenceLock.cpp
	Validator.cpp
	ActiveEncodingsManager.cpp
	EncoderVideoAudioProxy.cpp
	EncodersLoadBalancer.cpp
	EMailSender.cpp
)
SET (HEADERS
	Workspace.h
	MMSStorage.h
	MMSEngineDBFacade.h
	UpdaterEncoderJob.h
	UpdaterIngestionJob.h
	PersistenceLock.h
	Validator.h
	ActiveEncodingsManager.h
	EncoderVideoAudioProxy.h
	EncodersLoadBalancer.h
	EMailSender.h
	LocalAssetIngestionEvent.h
	MultiLocalAssetIngestionEvent.h
)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

include_directories ("${PROJECT_SOURCE_DIR}/FFMpeg/src")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/include")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/include")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/curlpp/include")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/include")
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	include_directories ("/usr/include/mysql-cppconn/jdbc")
endif()
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
	include_directories ("/usr/local/Cellar/imagemagick/7.0.7-22/include/ImageMagick-7")
else()
	include_directories ("${PROJECT_SOURCE_DIR}/../deploy/ImageMagick-7.0.8-49/include/ImageMagick-7")
	include_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/include/opencv4")
endif()

if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../usr_local/lib")
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
	target_link_libraries (MMSEngine MultiEventsSet)
endif()

install (TARGETS MMSEngine DESTINATION lib)
install (FILES ${HEADERS} DESTINATION include)

