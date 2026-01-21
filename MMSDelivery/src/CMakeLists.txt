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
  MMSDeliveryAuthorization.cpp
)
set(HEADERS
  MMSDeliveryAuthorization.h
)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(
  COMMAND ${LSB_RELEASE_EXEC} -is
  OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
  OUTPUT_STRIP_TRAILING_WHITESPACE)

include_directories("${STRINGUTILS_INCLUDE_DIR}")
include_directories("${ENCRYPT_INCLUDE_DIR}")
include_directories("${CONVERT_INCLUDE_DIR}")
include_directories("${JSONUTILS_INCLUDE_DIR}")
include_directories("${POSTGRESHELPER_INCLUDE_DIR}")
include_directories("${DBCONN_INCLUDE_DIR}")
include_directories("${CURLWRAPPER_INCLUDE_DIR}")
include_directories("${LIBPQXX_INCLUDE_DIR}")
include_directories("${SPDLOG_INCLUDE_DIR}")
include_directories("${THREADLOGGER_INCLUDE_DIR}")
include_directories("${NLOHMANN_INCLUDE_DIR}")
include_directories("${BANDWIDTHUSAGETHREAD_INCLUDE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/MMSStorage/src")
include_directories("${PROJECT_SOURCE_DIR}/MMSEngine/src")
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")

if(APPLE)
    include_directories("${OPENSSL_INCLUDE_DIR}")
endif()

if(APPLE)
    link_directories("${OPENSSL_LIB_DIR}")
    link_directories("${LIBPQXX_LIB_DIR}")
    link_directories("${LIBPQ_LIB_DIR}")
endif()

add_library(MMSDelivery SHARED ${SOURCES} ${HEADERS})

target_link_libraries(MMSDelivery BandwidthUsageThread)
if(APPLE)
    target_link_libraries(MMSDelivery MMSStorage)
    target_link_libraries(MMSDelivery FFMpegWrapper)
    target_link_libraries(MMSDelivery crypto)
endif()

install(TARGETS MMSDelivery DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)
