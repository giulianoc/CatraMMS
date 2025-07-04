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

#servers.sh non è stato inserito perchè non deve essere deployed nei server di pre e prod
set(SCRIPTs
    mmsApi.sh
    mmsDelivery.sh
    mmsExternalDelivery.sh
    mmsEncoder.sh
    mmsEnv.sh
    mmsEngineService.sh
    ffmpegHelper.sh
    imageMagickHelper.sh
    nginx.sh
    tomcat.sh
    setupALL.sh
    micro-service-deploy.sh
    micro-service.sh
    mmsStatusALL.sh
    mmsStartALL.sh
    mmsStopALL.sh
    mmsTail.sh
    printLogFileName.sh
    crontab.sh
    incrontab.sh
    incronServiceCheck.sh
    sqlQueries.txt
    checkStatistics.sh
    checkConsistencyFileSystem-Database.sh
    tvController.sh
    tvControllerLoop.sh
    tvController.sh.1
    deploy.sh
    installServer.sh
    getIngestionsToBeManaged.sh
    servicesStatusAgent.sh
    servicesStatusLibrary.sh
    loopServicesStatus.sh
    startRecorder.sh
    switchTranscoderInternalExternal.sh
    backupStorage.sh
    cibortvStartChannelsManualLoop.sh
    cibortvStartChannelsAutoLoop.sh)

install(
  FILES ${SCRIPTs}
  DESTINATION scripts
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_READ GROUP_EXECUTE
              WORLD_READ)
