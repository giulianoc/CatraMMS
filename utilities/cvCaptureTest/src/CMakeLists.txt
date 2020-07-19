
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
	cvCaptureTest.cpp
)

SET (HEADERS
)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
endif()

include_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/include/opencv4")

link_directories ("${PROJECT_SOURCE_DIR}/../deploy/opencv/lib64")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
	#ffmpeg-4.2.2 is missing because libopencv needs the one installed by yum (so inside the /opt/lib dir)
	# that it is ffmpeg-4.1.3
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
	link_directories ("${PROJECT_SOURCE_DIR}/../deploy/ffmpeg-4.2.2/lib")
endif()

add_executable(cvCaptureTest ${SOURCES} ${HEADERS})

if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
elseif(LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu")
	target_link_libraries (cvCaptureTest aom)
	target_link_libraries (cvCaptureTest fdk-aac)
	target_link_libraries (cvCaptureTest x264)
	target_link_libraries (cvCaptureTest x265)
	target_link_libraries (cvCaptureTest srt)
endif()
if(CRYPTO_FOUND)
	if (APPLE)
		target_link_libraries (cvCaptureTest ${CRYPTO_LIBRARIES})
	else()
		target_link_libraries (cvCaptureTest crypt)
	endif()
endif()
if (ZLIB_FOUND)
	target_link_libraries(cvCaptureTest ${ZLIB_LIBRARIES})
endif()
target_link_libraries (cvCaptureTest opencv_core)
target_link_libraries (cvCaptureTest opencv_imgcodecs)
target_link_libraries (cvCaptureTest opencv_imgproc)
target_link_libraries (cvCaptureTest opencv_videoio)
target_link_libraries (cvCaptureTest opencv_video)
target_link_libraries (cvCaptureTest opencv_videostab)
target_link_libraries (cvCaptureTest opencv_objdetect)
target_link_libraries (cvCaptureTest opencv_face)
target_link_libraries (cvCaptureTest avcodec)
target_link_libraries (cvCaptureTest avformat)
target_link_libraries (cvCaptureTest avutil)
target_link_libraries (cvCaptureTest swscale)
target_link_libraries (cvCaptureTest swresample)

install (TARGETS cvCaptureTest DESTINATION bin)

