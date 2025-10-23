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

#cmake_minimum_required(VERSION 2.8.12)
cmake_minimum_required(VERSION 3.30.2)

project(CatraMMS)

# generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE INTERNAL "")

set(CMAKE_CXX_FLAGS "-std=c++20")

add_compile_definitions(COMPRESSOR)

# this is to generate a compilation database (for the YCM vim plugin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-D__POSTGRES__)
# next serve per una condizione in MMSEngine/src/CMakeLists.txt
set(__POSTGRES__ "1")

# set(CMAKE_BUILD_TYPE Debug) set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_BUILD_TYPE Release)

set(CatraMMS_VERSION_MAJOR 1)
set(CatraMMS_VERSION_MINOR 0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(LibCrypto)
# $(LibCrypto_INCLUDE_DIRS) and $(LibCrypto_LIBS) availables
find_package(ZLIB)
# $(ZLIB_INCLUDE_DIRS) and $(ZLIB_LIBS) availables
find_package(aws-cpp-sdk-ec2)
# $(aws-cpp-sdk-ec2_INCLUDE_DIRS) and $(aws-cpp-sdk-ec2_LIBS) availables

#Le macro SPDLOG_DEBUG, SPDLOG_TRACE sono controllate at Compile-time dal define SPDLOG_ACTIVE_LEVEL.
#Di default è SPDLOG_LEVEL_INFO.
#Se non si cambia questa define a SPDLOG_LEVEL_DEBUG o SPDLOG_LEVEL_TRACE al momento della compilazione,
#le chiamate a SPDLOG_DEBUG e SPDLOG_TRACE vengono eliminate dal codice già in fase di compilazione.
add_compile_options(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

add_compile_options(-D_FILE_OFFSET_BITS=64)
# Next Magick defines are just to avoid compilation warnings
add_compile_options(-DMAGICKCORE_HDRI_ENABLE=0)
add_compile_options(-DMAGICKCORE_QUANTUM_DEPTH=16)

#add_compile_options(-DSAFEFILESYSTEMPROCESS)
#add_compile_options(-DSAFEFILESYSTEMTHREAD)

option(DBCONNECTION_MYSQL "Enable MySQL support" OFF)
if(APPLE)
    set(FCGI_INCLUDE_DIR "/opt/homebrew/Cellar/fcgi/2.4.6/include")
    set(FCGI_LIB_DIR "/opt/homebrew/Cellar/fcgi/2.4.6/lib")
    set(OPENSSL_INCLUDE_DIR "/opt/homebrew/Cellar/openssl@3/3.6.0/include")
    set(OPENSSL_LIB_DIR "/opt/homebrew/Cellar/openssl@3/3.6.0/lib")
    set(LDAP_INCLUDE_DIR "/opt/homebrew/Cellar/openldap/2.6.10_1/include")
    set(LDAP_LIB_DIR "/opt/homebrew/Cellar/openldap/2.6.10_1/lib")
    set(LIBPQ_LIB_DIR "/opt/homebrew/opt/libpq/lib")
    set(OPENCV_INCLUDE_DIR "/opt/homebrew/Cellar/opencv/4.12.0_12/include/opencv4")
    set(OPENCV_LIB_DIR "/opt/homebrew/Cellar/opencv/4.12.0_12/lib")
    set(FFMPEG_LIB_DIR "/opt/homebrew/Cellar/ffmpeg/8.0_1/lib")
    set(IMAGEMAGICK_INCLUDE_DIR "/opt/homebrew/Cellar/imagemagick/7.1.2-7/include/ImageMagick-7")
    set(IMAGEMAGICK_LIB_DIR "/opt/homebrew/Cellar/imagemagick/7.1.2-7/lib")
    set(AWSSDK_INCLUDE_DIR "/opt/homebrew/Cellar/aws-sdk-cpp/1.11.660/include")
    set(AWSSDK_LIB_DIR "/opt/homebrew/Cellar/aws-sdk-cpp/1.11.660/lib")
    set(AWSCRT_INCLUDE_DIR "/opt/homebrew/Cellar/aws-crt-cpp/0.35.0/include")
    set(AWSCOMMON_INCLUDE_DIR "/opt/homebrew/Cellar/aws-c-common/0.12.5/include")
    set(AWSIO_INCLUDE_DIR "/opt/homebrew/Cellar/aws-c-io/0.23.2/include")
    set(AWSMQTT_INCLUDE_DIR "/opt/homebrew/Cellar/aws-c-mqtt/0.13.3/include")
    set(AWSAUTH_INCLUDE_DIR "/opt/homebrew/Cellar/aws-c-auth/0.9.1/include")
    set(AWSUTILS_INCLUDE_DIR "/opt/homebrew/Cellar/aws-c-sdkutils/0.2.4/include")
endif()
if(APPLE)
    set(SPDLOG_INCLUDE_DIR "/Users/giuliano/rsi/zorac-dev/deploy/spdlog/include")
    set(NLOHMANN_INCLUDE_DIR "/Users/giuliano/rsi/zorac-dev/deploy/nlohmann/include")
    set(LIBPQXX_INCLUDE_DIR "/Users/giuliano/rsi/zorac-dev/deploy/libpqxx/include")
    set(LIBPQXX_LIB_DIR "/Users/giuliano/rsi/zorac-dev/deploy/libpqxx/lib")
else()
    set(SPDLOG_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/../deploy/spdlog/include")
    set(NLOHMANN_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/../deploy/nlohmann/include")
    set(LIBPQXX_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/../deploy/libpqxx/include")
    set(LIBPQXX_LIB_DIR "${PROJECT_SOURCE_DIR}/../deploy/libpqxx/lib")
endif()
set(POSTGRESHELPER_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/PostgresHelper/src")
set(STATISTICTIMER_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/StatisticTimer/src")
set(STRINGUTILS_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/StringUtils/src")
set(ERROR_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/Error/src")
set(ERROR_LIB_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/Error/src")
set(PTHREAD_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/PThread/src")
set(PTHREAD_LIB_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/PThread/src")
set(GETCPUUSAGE_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/GetCpuUsage/src")
set(ENCRYPT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/Encrypt/src")
set(LDAPWRAPPER_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/LdapWrapper/src")
set(SYSTEM_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/System/src")
set(FASTCGIAPI_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/FastCGIAPI/src")
set(CONVERT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/Convert/src")
set(DATETIME_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/Datetime/src")
set(DATETIME_LIB_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/Datetime/src")
set(EVENTSSET_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/EventsSet/src")
set(MULTIEVENTSSET_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/MultiEventsSet/src")
set(MULTIEVENTSSET_LIB_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/MultiEventsSet/src")
set(SCHEDULER_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/Scheduler/src")
set(JSONUTILS_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/JSONUtils/src")
set(CURLWRAPPER_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/CurlWrapper/src")
set(FFMPEGWRAPPER_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/FFMpegWrapper/src")
set(PROCESSUTILITY_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/ProcessUtility/src")
set(DBCONN_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/DBConnectionsPool/src")
set(SAFEFILESYSTEM_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/CatraLibraries/SafeFileSystem/src")
add_subdirectory(CatraLibraries/StringUtils/src)
add_subdirectory(CatraLibraries/Encrypt/src)
#add_subdirectory(CatraLibraries/Encrypt/examples/encrypt)
#add_subdirectory(CatraLibraries/Encrypt/examples/decrypt)
add_subdirectory(CatraLibraries/LdapWrapper/src)
add_subdirectory(CatraLibraries/System/src)
#add_subdirectory(CatraLibraries/System/examples/getBandwidthInMbps)
add_subdirectory(CatraLibraries/Convert/src)
add_subdirectory(CatraLibraries/Datetime)
add_subdirectory(CatraLibraries/Error/src)
add_subdirectory(CatraLibraries/PThread/src)
add_subdirectory(CatraLibraries/Scheduler/src)
add_subdirectory(CatraLibraries/EventsSet/src)
add_subdirectory(CatraLibraries/MultiEventsSet/src)
add_subdirectory(CatraLibraries/GetCpuUsage/src)
add_subdirectory(CatraLibraries/JSONUtils)
add_subdirectory(CatraLibraries/PostgresHelper/src)
add_subdirectory(CatraLibraries/CurlWrapper/src)
#add_subdirectory(CatraLibraries/CurlWrapper/examples/escape)
add_subdirectory(CatraLibraries/FastCGIAPI/src)
add_subdirectory(CatraLibraries/ProcessUtility/src)
add_subdirectory(CatraLibraries/FFMpegWrapper/src)
add_subdirectory(CatraLibraries/StatisticTimer/src)
add_subdirectory(CatraLibraries/SafeFileSystem/src)

add_subdirectory(API/src)
add_subdirectory(FFMPEGEncoder/src)

add_subdirectory(MMSEngine/src)
add_subdirectory(MMSDelivery/src)
add_subdirectory(MMSProxy/src)
add_subdirectory(MMSStorage/src)
add_subdirectory(MMSEngineService/src)

add_subdirectory(conf)
add_subdirectory(ttfFont)
add_subdirectory(docs)
add_subdirectory(docs/howTo)
add_subdirectory(maintenanceEmails)
add_subdirectory(scripts)
add_subdirectory(generateHtmlDoc)
add_subdirectory(jsonSamples)
add_subdirectory(predefinedEncodingProfiles/video)
add_subdirectory(predefinedEncodingProfiles/audio)
add_subdirectory(predefinedEncodingProfiles/image)
add_subdirectory(predefinedWorkflowLibrary)
add_subdirectory(predefinedImages)

add_subdirectory(utilities/awsSigner/src)
# add_subdirectory(utilities/registerAndConfirmUser/src)
add_subdirectory(utilities/updateGEOInfo/src)
# add_subdirectory(utilities/ffmpeg_getLiveStreamingInfo/src)
# add_subdirectory(utilities/confirmUser)
# add_subdirectory(utilities/cvCaptureTest/src)
# add_subdirectory(utilities/lockTest/src)
add_subdirectory(utilities/sendEmail/src)
# add_subdirectory(utilities/addWorkspaceToExistingActiveDirectoryUser/src)

install(FILES version.txt DESTINATION ./)
