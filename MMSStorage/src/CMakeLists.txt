# Copyright (C) Giuliano Catrambone (giuliano.catrambone@catrasoftware.it)

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.

# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.

# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 675 Mass
# Ave, Cambridge, MA 02139, USA.

# Commercial use other than under the terms of the GNU General Public License is
# allowed only after express negotiation of conditions with the authors.

set(SOURCES
    MMSStorage.cpp
)
set(HEADERS
    MMSStorage.h
)

#add_compile_definitions(MAGICKCORE_HDRI_ENABLE=0)
#add_compile_definitions(MAGICKCORE_QUANTUM_DEPTH=16)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(
  COMMAND ${LSB_RELEASE_EXEC} -is
  OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
  OUTPUT_STRIP_TRAILING_WHITESPACE)

include_directories("${STRINGUTILS_INCLUDE_DIR}")
include_directories("${THREADLOGGER_INCLUDE_DIR}")
include_directories("${DATETIME_INCLUDE_DIR}")
include_directories("${SYSTEM_INCLUDE_DIR}")
include_directories("${JSONUTILS_INCLUDE_DIR}")
include_directories("${POSTGRESHELPER_INCLUDE_DIR}")
include_directories("${DBCONN_INCLUDE_DIR}")
include_directories("${PROCESSUTILITY_INCLUDE_DIR}")
include_directories("${SAFEFILESYSTEM_INCLUDE_DIR}")
include_directories("${LIBPQXX_INCLUDE_DIR}")
include_directories("${SPDLOG_INCLUDE_DIR}")
include_directories("${NLOHMANN_INCLUDE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/MMSEngine/src")
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
  include_directories("/usr/include/mysql-cppconn/jdbc")
endif()

if(APPLE)
  link_directories("${OPENSSL_LIB_DIR}")
  link_directories("${LIBPQXX_LIB_DIR}")
  link_directories("${LIBPQ_LIB_DIR}")
endif()

add_library(MMSStorage SHARED ${SOURCES} ${HEADERS})

if(APPLE)
  target_link_libraries(MMSStorage ProcessUtility)
  target_link_libraries(MMSStorage MMSEngine)
  target_link_libraries(MMSStorage System)
endif()

install(TARGETS MMSStorage DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)
