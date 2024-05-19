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

set(SOURCES PostgresHelper.cpp)
set(HEADERS PostgresHelper.h)

include_directories("${PROJECT_SOURCE_DIR}/../deploy/libpqxx/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/include")
include_directories("${PROJECT_SOURCE_DIR}/MMSUtility/src")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/spdlog/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/nlohmann/include")

add_library(postgresHelper SHARED ${SOURCES} ${HEADERS})

install(TARGETS postgresHelper DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)
