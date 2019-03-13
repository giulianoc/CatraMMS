
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
	mmsTest.cpp
)

SET (HEADERS
)

include_directories ("${PROJECT_SOURCE_DIR}/../deploy/include")
include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/include")
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
else()
	include_directories ("${PROJECT_SOURCE_DIR}/../deploy/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/include")
endif()

link_directories ("${PROJECT_SOURCE_DIR}/../deploy/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/curlpp/lib")
if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
else()
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ImageMagick-7.0.8-10/lib")
endif()
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg-4.0.2/lib")
link_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib")

add_executable(mmsTest ${SOURCES} ${HEADERS})

target_link_libraries (mmsTest MMSEngine)
target_link_libraries (mmsTest FFMpeg)
target_link_libraries (mmsTest curl)
target_link_libraries (mmsTest curlpp)
target_link_libraries (mmsTest mysqlcppconn)
target_link_libraries (mmsTest jsoncpp)
if (APPLE) 
else()  
        target_link_libraries (mmsTest Magick++-7.Q16HDRI)
endif() 


#target_link_libraries (mmsTest Scheduler)
#target_link_libraries (mmsTest PThread)
target_link_libraries (mmsTest Tools)
target_link_libraries (mmsTest ErrMsg)
target_link_libraries (mmsTest MultiEventsSet)
if(CRYPTO_FOUND)
	if (APPLE)
		target_link_libraries (mmsTest ${CRYPTO_LIBRARIES})
	else()
		target_link_libraries (mmsTest crypt)
	endif()
endif()
if (ZLIB_FOUND)
	target_link_libraries(mmsTest ${ZLIB_LIBRARIES})
endif()
target_link_libraries (mmsTest pthread)
target_link_libraries (mmsTest opencv_core)
target_link_libraries (mmsTest opencv_imgcodecs)
target_link_libraries (mmsTest opencv_imgproc)
target_link_libraries (mmsTest opencv_videoio)
target_link_libraries (mmsTest opencv_video)
target_link_libraries (mmsTest opencv_videostab)
target_link_libraries (mmsTest opencv_objdetect)
target_link_libraries (mmsTest opencv_face)

install (TARGETS mmsTest DESTINATION bin)

