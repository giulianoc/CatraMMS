
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

cmake_minimum_required(
	VERSION 2.8.12
)

project(
	CatraMMS
)

SET(CMAKE_CXX_FLAGS "-std=c++14")

#this is to generate a compilation database (for the YCM vim plugin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#ADD_DEFINITIONS(-D__LOCALENCODER__)


set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE RelWithDebInfo)
#set(CMAKE_BUILD_TYPE Release)

set(CatraMMS_VERSION_MAJOR 1)
set(CatraMMS_VERSION_MINOR 0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(LibCrypto)
find_package(ZLIB)

add_compile_options(-D_FILE_OFFSET_BITS=64)

add_subdirectory(MMSEngine/src)
add_subdirectory(FFMpeg/src)
add_subdirectory(MMSEngineService/src)

add_subdirectory(API/src)

add_subdirectory(conf)
add_subdirectory(ttfFont)
add_subdirectory(scripts)
add_subdirectory(generateHtmlDoc)
add_subdirectory(jsonSamples)
add_subdirectory(predefinedEncodingProfiles/video)
add_subdirectory(predefinedEncodingProfiles/audio)
add_subdirectory(predefinedEncodingProfiles/image)

add_subdirectory(utilities/registerAdministratorCustomer/src)
add_subdirectory(utilities/cvCaptureTest/src)
add_subdirectory(utilities/lockTest/src)
add_subdirectory(utilities/sendEmail/src)
add_subdirectory(utilities/addWorkspaceToActiveDirectoryUser/src)

