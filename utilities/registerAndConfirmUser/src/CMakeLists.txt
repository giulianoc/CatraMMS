
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
	registerAndConfirmUser.cpp
)

SET (HEADERS
)

include_directories ("${SPDLOG_INCLUDE_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/MMSEngine/src")
include_directories("${NLOHMANN_INCLUDE_DIR}")
include_directories("${JSONUTILS_INCLUDE_DIR}")
include_directories("${POSTGRESHELPER_INCLUDE_DIR}")
include_directories("${DBCONN_INCLUDE_DIR}")
include_directories("${LIBPQXX_INCLUDE_DIR}")

link_directories("${LIBPQXX_LIB_DIR}")
link_directories("${LIBPQ_LIB_DIR}")
if(APPLE)
	link_directories("${OPENSSL_LIB_DIR}")
endif()

add_executable(registerAndConfirmUser ${SOURCES} ${HEADERS})

target_link_libraries(registerAndConfirmUser MMSEngine)
#target_link_libraries(registerAndConfirmUser Encrypt)
target_link_libraries(registerAndConfirmUser PostgresHelper)
target_link_libraries(registerAndConfirmUser CurlWrapper)
target_link_libraries(registerAndConfirmUser Datetime)
target_link_libraries(registerAndConfirmUser FFMpegWrapper)
target_link_libraries(registerAndConfirmUser FFMpegEngine)
target_link_libraries(registerAndConfirmUser StringUtils)
target_link_libraries(registerAndConfirmUser ProcessUtility)
target_link_libraries(registerAndConfirmUser Convert)
target_link_libraries(registerAndConfirmUser Encrypt)
target_link_libraries(registerAndConfirmUser curl)
target_link_libraries(registerAndConfirmUser pqxx)
target_link_libraries(registerAndConfirmUser pq)

install (TARGETS registerAndConfirmUser DESTINATION bin)

