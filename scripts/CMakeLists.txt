
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

SET (SCRIPTs
	mmsApi.sh
	mmsApi.rsi.lab.sh
	mmsApi.rsi.mms2.sh
	mmsEncoder.sh
	mmsEncoder.rsi.lab.sh
	mmsEncoder.rsi.mms2.sh
	mmsEnv.sh
	mmsEngineService.sh
	mmsEngineService.rsi.lab.sh
	mmsEngineService.rsi.mms2.sh
	ffmpegHelper.sh
	imageMagickHelper.sh
	nginx.sh
	tomcat.sh
	setupALL.sh
	statusALL.sh
	startALL.sh
	stopALL.sh
	mmsTail.sh
	printLogFileName.sh
	crontab.sh
	crontab.rsi.sh
	sqlQueries.txt
)

install (
	FILES ${SCRIPTs} 
	DESTINATION scripts 
	PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_READ GROUP_EXECUTE WORLD_READ
)

