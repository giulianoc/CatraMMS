
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

SET (COMMON_SOURCES
	APICommon.cpp
)

SET (SOURCES_API
	API.cpp
)

SET (SOURCES_BINARY
	Binary.cpp
)

SET (HEADERS
	APICommon.h
)

include_directories ("${PROJECT_SOURCE_DIR}/../usr_local/include")
include_directories ("${PROJECT_SOURCE_DIR}/../spdlog/include")
include_directories ("${PROJECT_SOURCE_DIR}/CMSEngine/src")
if (APPLE)
	include_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include")
else()
	include_directories ("/app/5/ThirdPartySoftware/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/include")
	include_directories ("${PROJECT_SOURCE_DIR}/API/src")
endif()

link_directories ("${PROJECT_SOURCE_DIR}/../usr_local/lib")
if (APPLE)
	link_directories ("${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/lib")
else()
	link_directories ("/app/5/ThirdPartySoftware/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/lib")
endif()

#API
    add_executable(API.fcgi API.cpp ${COMMON_SOURCES} ${HEADERS})
    target_link_libraries (API.fcgi CMSEngine)
    target_link_libraries (API.fcgi mysqlcppconn)
    target_link_libraries (API.fcgi jsoncpp)
    if (APPLE)
    else()
	    target_link_libraries (API.fcgi Magick++-7.Q16HDRI)
    endif()
    target_link_libraries(API.fcgi fcgi++)
    target_link_libraries(API.fcgi curl)
    target_link_libraries(API.fcgi fcgi)
    target_link_libraries (API.fcgi Tools)
target_link_libraries (API.fcgi ErrMsg)
    if(CRYPTO_FOUND)
	    if (APPLE)
		    target_link_libraries (API.fcgi ${CRYPTO_LIBRARIES})
	    else()
		target_link_libraries (API.fcgi crypt)
	    endif()
    endif()
    if (ZLIB_FOUND)
	    target_link_libraries(API.fcgi ${ZLIB_LIBRARIES})
    endif()
    target_link_libraries (API.fcgi pthread)
    list(APPEND TARGETS API.fcgi)

#Binary
    add_executable(Binary.cgi Binary.cpp ${COMMON_SOURCES} ${HEADERS})
    target_link_libraries (Binary.cgi CMSEngine)
    target_link_libraries (Binary.cgi mysqlcppconn)
    target_link_libraries (Binary.cgi jsoncpp)
    if (APPLE)
    else()
	    target_link_libraries (Binary.cgi Magick++-7.Q16HDRI)
    endif()
    target_link_libraries(Binary.cgi fcgi++)
    target_link_libraries(Binary.cgi curl)
    target_link_libraries(Binary.cgi fcgi)
    target_link_libraries (Binary.cgi Tools)
target_link_libraries (Binary.cgi ErrMsg)
    if(CRYPTO_FOUND)
	    if (APPLE)
		    target_link_libraries (Binary.cgi ${CRYPTO_LIBRARIES})
	    else()
		target_link_libraries (Binary.cgi crypt)
	    endif()
    endif()
    if (ZLIB_FOUND)
	    target_link_libraries(Binary.cgi ${ZLIB_LIBRARIES})
    endif()
    target_link_libraries (Binary.cgi pthread)
    list(APPEND TARGETS Binary.cgi)


install (TARGETS DESTINATION bin)

