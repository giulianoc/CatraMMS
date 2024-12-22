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

if(DEFINED __POSTGRES__)
  set(SOURCES
      AWSSigner.cpp
      MMSDeliveryAuthorization.cpp
      MMSStorage.cpp
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
      MMSEngineDBFacade_Statistic_Postgres.cpp
      MMSEngineDBFacade_UserWorkspace_Postgres.cpp
      MMSEngineDBFacade_WorkflowLibrary_Postgres.cpp
      MMSEngineDBFacade_Workflow_Postgres.cpp
      Workspace.cpp
      Validator.cpp
      ActiveEncodingsManager.cpp
      EncoderProxy.cpp
      EncoderProxy_Content.cpp
      EncoderProxy_FaceIdentification.cpp
      EncoderProxy_FaceRecognition.cpp
      EncoderProxy_Image.cpp
      EncoderProxy_LiveProxy.cpp
      EncoderProxy_LiveRecorder.cpp
      EncoderProxy_Process.cpp
      EncoderProxy_AWS.cpp
      EncodersLoadBalancer.cpp
      EMailSender.cpp
      ThreadsStatistic.cpp)
else()
  set(SOURCES
      AWSSigner.cpp
      MMSDeliveryAuthorization.cpp
      MMSStorage.cpp
      mysql/MMSEngineDBFacade.cpp
      mysql/MMSEngineDBFacade_Configuration.cpp
      mysql/MMSEngineDBFacade_CreateTables.cpp
      mysql/MMSEngineDBFacade_EncodersPool.cpp
      mysql/MMSEngineDBFacade_EncodingJobs.cpp
      mysql/MMSEngineDBFacade_EncodingProfiles.cpp
      mysql/MMSEngineDBFacade_IngestionJobs.cpp
      mysql/MMSEngineDBFacade_Lock.cpp
      mysql/MMSEngineDBFacade_MediaItems.cpp
      mysql/MMSEngineDBFacade_PartitionInfo.cpp
      mysql/MMSEngineDBFacade_Statistic_Postgres.cpp
      mysql/MMSEngineDBFacade_UserWorkspace.cpp
      mysql/MMSEngineDBFacade_WorkflowLibrary.cpp
      PersistenceLock.cpp
      Workspace.cpp
      Validator.cpp
      ActiveEncodingsManager.cpp
      EncoderVideoAudioProxy.cpp
      EncodersLoadBalancer.cpp
      EMailSender.cpp
      ThreadsStatistic.cpp)
endif()
set(HEADERS
    AWSSigner.h
    MMSDeliveryAuthorization.h
    MMSStorage.h
    MMSEngineDBFacade.h
    PersistenceLock.h
    Workspace.h
    Validator.h
    ActiveEncodingsManager.h
    EncoderProxy.h
    EncodersLoadBalancer.h
    EMailSender.h
    LocalAssetIngestionEvent.h
    MultiLocalAssetIngestionEvent.h
    ThreadsStatistic.h)

add_compile_definitions(MAGICKCORE_HDRI_ENABLE=0)
add_compile_definitions(MAGICKCORE_QUANTUM_DEPTH=16)

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(
  COMMAND ${LSB_RELEASE_EXEC} -is
  OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
  OUTPUT_STRIP_TRAILING_WHITESPACE)

include_directories("${PROJECT_SOURCE_DIR}/MMSUtility/src")
include_directories("${PROJECT_SOURCE_DIR}/FFMpeg/src")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/libpqxx/include")
include_directories("${PROJECT_SOURCE_DIR}/PostgresHelper/src")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/spdlog/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/CatraLibraries/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/curlpp/include")
include_directories("${PROJECT_SOURCE_DIR}/../deploy/nlohmann/include")
message("-- LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
if(LSB_RELEASE_ID_SHORT STREQUAL "CentOS")
  include_directories("/usr/include/mysql-cppconn/jdbc")
endif()
if(APPLE)
  include_directories("${PROJECT_SOURCE_DIR}/MMSEngine/src")
  include_directories(
    "${PROJECT_SOURCE_DIR}/../mysql-connector-c++-1.1.9-macos10.12-x86-64bit/include"
  )
  include_directories(
    "/usr/local/Cellar/imagemagick/7.0.7-22/include/mageMagick-7")
else()
  # ImageMagick, inside the deploy dir, is a link to the real version:
  # ImageMagick -> ImageMagick-7.0.10
  include_directories(
    "${PROJECT_SOURCE_DIR}/../deploy/ImageMagick/include/ImageMagick-7")
  include_directories("${PROJECT_SOURCE_DIR}/../deploy/opencv/include/opencv4")
endif()

if(APPLE)
  link_directories("${PROJECT_SOURCE_DIR}/../usr_local/lib")
endif()
link_directories("${PROJECT_SOURCE_DIR}/../deploy/aws-sdk-cpp/lib")
link_directories("${PROJECT_SOURCE_DIR}/../deploy/libpqxx/lib")

add_library(MMSEngine SHARED ${SOURCES} ${HEADERS})

target_link_libraries(MMSEngine aws-cpp-sdk-core)
target_link_libraries(MMSEngine aws-cpp-sdk-medialive)
if(APPLE)
  target_link_libraries(MMSEngine FFMpeg)
  target_link_libraries(MMSEngine ErrMsg)
  target_link_libraries(MMSEngine Tools)
  target_link_libraries(MMSEngine mysqlcppconn)
  target_link_libraries(MMSEngine jsoncpp)
  target_link_libraries(MMSEngine curl)
  target_link_libraries(MMSEngine curlpp)
  target_link_libraries(MMSEngine Magick++-7.Q16HDRI)
  target_link_libraries(MMSEngine MultiEventsSet)
endif()

install(TARGETS MMSEngine DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)
