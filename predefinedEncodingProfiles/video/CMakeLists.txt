
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

SET (PROFILEs
	MMS_MP4_H264_medium_360p25_AAC_92.json
	MMS_MP4_H264_veryfast_360p25_AAC_92.json
	MMS_MP4_H264_veryfast_360p25_high422_AAC_92.json
	MMS_TS_H264_veryfast_360p25_high422_AAC_92.json
	MMS_HLS_H264_veryfast_360p25_high422_AAC_92.json
	MMS_DASH_H264_veryfast_360p25_high422_AAC_92.json
	MMS_MP4_H264_veryslow_360p25_AAC_92.json
	MMS_MP4_AV1_ConstrainedQuality_2Mbps_360p_AAC_92.json
	MMS_MP4_AV1_ConstantQuality_360p_AAC_92.json
	MMS_MP4_H264_veryfast_720p50_high422_AAC_92.json
	MMS_MP4_H264_veryslow_720p25_AAC_160.json
	MMS_HLS_H264_veryslow_720p25_AAC_160.json
	MMS_DASH_H264_veryslow_720p25_AAC_160.json
	MMS_MP4_H264_6Mbps_veryslow_720p50_AAC_160.json
	MMS_MP4_H264_6Mbps_veryslow_720p25_AAC_160.json
	MMS_MP4_H264_veryfast_1080p50_high422_AAC_128.json
)

install (FILES ${PROFILEs} DESTINATION predefinedEncodingProfiles/video)

