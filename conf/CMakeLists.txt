
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

SET (CONFs
	nginx.conf
	catramms.nginx
	catrammsAPIServer.nginx
	catrammsWEBAPIServer.nginx
	catrammsGUIServer.nginx
	catrammsDeliveryServer.nginx
	catrammsDelivery-pathServer.nginx
	catrammsDelivery-fServer.nginx
	catrammsBinaryServer.nginx
	catrammsEncoderServer.nginx
	mms-env.sh.example
	mms.cfg
	crontab.txt
	3_terrestrial_2022_09_07.channel.dvbv5.conf
	3_terrestrial_2024_03_01.channel.dvbv5.conf
	3_terrestrial_2024_12_19.channel.dvbv5.conf
	3_EXTENDED_2021_07_19.channel.dvbv5.conf
	3_UNIVERSAL_2021_07_19.channel.dvbv5.conf
	3_UNIVERSAL.channel.dvbv5.conf
	partitionInfo.json
	pk-APKAUYWFOBAADUMU4IGK.pem.gz
)

install (FILES ${CONFs} DESTINATION conf)

