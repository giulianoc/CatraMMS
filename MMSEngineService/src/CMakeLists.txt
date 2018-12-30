
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
)

SET (HEADERS
	MMSEngineProcessor.h
	CheckIngestionTimes.h
	CheckEncodingTimes.h
	RetentionTimes.h
)

include_directories ("${PROJECT_SOURCE_DIR}/../spdlog/include")
include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
include_directories ("${PROJECT_SOURCE_DIR}/FFMpeg/src")
include_directories ("${PROJECT_SOURCE_DIR}/../usr_local/include")
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
	include_directories ("/usr/local/Cellar/imagemagick/7.0.7-22/include/ImageMagick-7")
else()
	include_directories ("/app/5/ThirdPartySoftware/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/include")
	include_directories ("${PROJECT_SOURCE_DIR}/../usr_local/include/ImageMagick-7")
endif()

link_directories ("${PROJECT_SOURCE_DIR}/../usr_local/lib")
if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
else()
	link_directories ("/app/5/ThirdPartySoftware/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/lib")
	link_directories ("/home/giuliano/catramms/opencv/lib")
endif()

add_executable(mmsEngineService ${SOURCES} ${HEADERS})

target_link_libraries (mmsEngineService MultiEventsSet)
target_link_libraries (mmsEngineService MMSEngine)
target_link_libraries (mmsEngineService FFMpeg)
target_link_libraries (mmsEngineService mysqlcppconn)
target_link_libraries (mmsEngineService jsoncpp)
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
target_link_libraries (mmsEngineService avcodec)
#target_link_libraries (mmsEngineService avformat)
#target_link_libraries (mmsEngineService avutil)

install (TARGETS mmsEngineService DESTINATION bin)
