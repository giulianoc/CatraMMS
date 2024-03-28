
SET (SOURCES
	FastCGIAPI.cpp
	Compressor.cpp
)
SET (HEADERS
	FastCGIAPI.h
	Compressor.h
)

include_directories ("${PROJECT_SOURCE_DIR}/MMSUtility/src")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/spdlog/include")
include_directories ("${PROJECT_SOURCE_DIR}/../deploy/nlohmann/include")

add_library (fastCGIAPI SHARED ${SOURCES} ${HEADERS})

install (TARGETS fastCGIAPI DESTINATION lib)
install (FILES ${HEADERS} DESTINATION include)

