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
      AWSSigner.cpp
      #MMSDeliveryAuthorization.cpp
      MMSEngineDBFacade_Postgres.cpp
      MMSEngineDBFacade_Configuration_Postgres.cpp
      MMSEngineDBFacade_ConfigurationOutputType_Postgres.cpp
      MMSEngineDBFacade_ConfigurationSocial_Postgres.cpp
      MMSEngineDBFacade_ConfigurationStream_Postgres.cpp
      MMSEngineDBFacade_CreateTables_Postgres.cpp
      MMSEngineDBFacade_EncodersPool_Postgres.cpp
      MMSEngineDBFacade_EncodingJobs_Postgres.cpp
      MMSEngineDBFacade_EncodingProfiles_Postgres.cpp
      MMSEngineDBFacade_IngestionJobs_Postgres.cpp
      MMSEngineDBFacade_Invoice_Postgres.cpp
      MMSEngineDBFacade_MediaItems_Postgres.cpp
      MMSEngineDBFacade_PartitionInfo_Postgres.cpp
      MMSEngineDBFacade_Retention_Postgres.cpp
      MMSEngineDBFacade_Statistic_Postgres.cpp
      MMSEngineDBFacade_UserWorkspace_Postgres.cpp
      MMSEngineDBFacade_WorkflowLibrary_Postgres.cpp
      MMSEngineDBFacade_Workflow_Postgres.cpp
      #MMSStorage.cpp
      Workspace.cpp
      Validator.cpp
      #ActiveEncodingsManager.cpp
      #EncoderProxy.cpp
      #EncoderProxy_Content.cpp
      #EncoderProxy_FaceIdentification.cpp
      #EncoderProxy_FaceRecognition.cpp
      #EncoderProxy_Image.cpp
      #EncoderProxy_LiveProxy.cpp
      #EncoderProxy_LiveRecorder.cpp
      #EncoderProxy_Process.cpp
      #EncoderProxy_AWS.cpp
      #EncodersLoadBalancer.cpp
      EMailSender.cpp
      ThreadsStatistic.cpp)
set(HEADERS
    AWSSigner.h
    #MMSDeliveryAuthorization.h
    MMSEngineDBFacade.h
    #MMSStorage.h
    PersistenceLock.h
    Workspace.h
    Validator.h
    #ActiveEncodingsManager.h
    #EncoderProxy.h
    #EncodersLoadBalancer.h
    EMailSender.h
    LocalAssetIngestionEvent.h
    MultiLocalAssetIngestionEvent.h
    ThreadsStatistic.h)

#add_compile_definitions(MAGICKCORE_HDRI_ENABLE=0)
#add_compile_definitions(MAGICKCORE_QUANTUM_DEPTH=16)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(
  COMMAND ${LSB_RELEASE_EXEC} -is
  OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
  OUTPUT_STRIP_TRAILING_WHITESPACE)

include_directories("${STRINGUTILS_INCLUDE_DIR}")
include_directories("${CONVERT_INCLUDE_DIR}")
include_directories("${DATETIME_INCLUDE_DIR}")
include_directories("${ENCRYPT_INCLUDE_DIR}")
#include_directories("${SYSTEM_INCLUDE_DIR}")
include_directories("${JSONUTILS_INCLUDE_DIR}")
#include_directories("${MULTIEVENTSSET_INCLUDE_DIR}")
include_directories("${POSTGRESHELPER_INCLUDE_DIR}")
include_directories("${DBCONN_INCLUDE_DIR}")
include_directories("${CURLWRAPPER_INCLUDE_DIR}")
include_directories("${FFMPEGENGINE_INCLUDE_DIR}")
include_directories("${FFMPEGWRAPPER_INCLUDE_DIR}")
include_directories("${PROCESSUTILITY_INCLUDE_DIR}")
#include_directories("${SAFEFILESYSTEM_INCLUDE_DIR}")
include_directories("${LIBPQXX_INCLUDE_DIR}")
include_directories("${SPDLOG_INCLUDE_DIR}")
include_directories("${NLOHMANN_INCLUDE_DIR}")
#include_directories("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/include")
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
  include_directories("/usr/include/mysql-cppconn/jdbc")
endif()
#if(APPLE)
#include_directories("${PROJECT_SOURCE_DIR}/MMSEngine/src")
#include_directories(
#"${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include"
#)
#include_directories(
#"/usr/local/Cellar/imagemagick/7.0.7-22/include/mageMagick-7")
#else()
## ImageMagick, inside the deploy dir, is a link to the real version:
## ImageMagick -> ImageMagick-7.0.10
#include_directories(
#"${PROJECT_SOURCE_DIR}/../deploy/ImageMagick/include/ImageMagick-7")
#include_directories("${PROJECT_SOURCE_DIR}/../deploy/opencv/include/opencv4")
#endif()

#if(APPLE)
#link_directories("${PROJECT_SOURCE_DIR}/../usr_local/lib")
#endif()
#link_directories("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/lib")
#link_directories("${PROJECT_SOURCE_DIR}/../deploy/libpqxx/lib")

if(APPLE)
  include_directories("${OPENSSL_INCLUDE_DIR}")
endif()

if(APPLE)
  link_directories("${CURLWRAPPER_LIB_DIR}")
  link_directories("${STRINGUTILS_LIB_DIR}")
  link_directories("${OPENSSL_LIB_DIR}")
  link_directories("${LIBPQXX_LIB_DIR}")
  link_directories("${LIBPQ_LIB_DIR}")
endif()

add_library(MMSEngine SHARED ${SOURCES} ${HEADERS})

if(DBCONNECTION_MYSQL)
  target_compile_definitions(MMSEngine PRIVATE DBCONNECTION_MYSQL)
endif()

if(APPLE)
  target_link_libraries(MMSEngine CurlWrapper)
  target_link_libraries(MMSEngine FFMpegWrapper)
  target_link_libraries(MMSEngine PostgresHelper)
  target_link_libraries(MMSEngine StringUtils)
  target_link_libraries(MMSEngine Encrypt)
  target_link_libraries(MMSEngine crypto)
endif()

#target_link_libraries(MMSEngine aws-cpp-sdk-core)
#target_link_libraries(MMSEngine aws-cpp-sdk-medialive)
#if(APPLE)
#target_link_libraries(MMSEngine FFMpeg)
#target_link_libraries(MMSEngine mysqlcppconn)
#target_link_libraries(MMSEngine jsoncpp)
#target_link_libraries(MMSEngine curl)
#target_link_libraries(MMSEngine Magick++-7.Q16HDRI)
#target_link_libraries(MMSEngine MultiEventsSet)
#endif()

install(TARGETS MMSEngine DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)
