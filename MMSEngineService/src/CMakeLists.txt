
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
	mmsEngineService.cpp
	MMSEngineProcessor.cpp
	CheckIngestionTimes.cpp
	CheckEncodingTimes.cpp
	RetentionTimes.cpp
	MainAndBackupRunningHALiveRecordingEvent.cpp
)

SET (HEADERS
	MMSEngineProcessor.h
	CheckIngestionTimes.h
	CheckEncodingTimes.h
	RetentionTimes.h
	MainAndBackupRunningHALiveRecordingEvent.h
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
	include_directories ("/usr/local/Cellar/imagemagick/7.0.7-22/include/ImageMagick-7")
else()
	include_directories ("${PROJECT_SOURCE_DIR}/../deploy/ImageMagick-7.0.8-49/include/ImageMagick-7")
endif()

link_directories ("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ImageMagick-7.0.8-49/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg-4.1.3/lib")
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
endif()

add_executable(mmsEngineService ${SOURCES} ${HEADERS})

target_link_libraries (mmsEngineService MultiEventsSet)
target_link_libraries (mmsEngineService MMSEngine)
target_link_libraries (mmsEngineService FFMpeg)
target_link_libraries (mmsEngineService mysqlcppconn)
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	target_link_libraries (mmsEngineService jsoncpp)
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
	target_link_libraries (mmsEngineService "${PROJECT_SOURCE_DIR}/../deploy/jsoncpp/lib/libjsoncpp.a")
endif()
target_link_libraries (mmsEngineService curl)
target_link_libraries (mmsEngineService curlpp)
if (APPLE) 
else()  
	target_link_libraries (mmsEngineService Magick++-7.Q16HDRI)
	target_link_libraries (mmsEngineService MagickCore-7.Q16HDRI)
	target_link_libraries (mmsEngineService MagickWand-7.Q16HDRI)
endif() 

target_link_libraries (mmsEngineService Scheduler)
target_link_libraries (mmsEngineService PThread)
target_link_libraries (mmsEngineService Tools)
target_link_libraries (mmsEngineService ErrMsg)
if(CRYPTO_FOUND)
	if (APPLE)
		target_link_libraries (mmsEngineService ${CRYPTO_LIBRARIES})
	else()
		target_link_libraries (mmsEngineService crypt)
	endif()
endif()
if (ZLIB_FOUND)
	target_link_libraries(mmsEngineService ${ZLIB_LIBRARIES})
endif()
target_link_libraries (mmsEngineService pthread)
target_link_libraries (mmsEngineService opencv_core)
target_link_libraries (mmsEngineService opencv_imgcodecs)
target_link_libraries (mmsEngineService opencv_imgproc)
target_link_libraries (mmsEngineService opencv_videoio)
target_link_libraries (mmsEngineService opencv_video)
target_link_libraries (mmsEngineService opencv_videostab)
target_link_libraries (mmsEngineService opencv_objdetect)
target_link_libraries (mmsEngineService opencv_face)
target_link_libraries (mmsEngineService avcodec)
#target_link_libraries (mmsEngineService avformat)
#target_link_libraries (mmsEngineService avutil)

install (TARGETS mmsEngineService DESTINATION bin)
