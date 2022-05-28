
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
	MMS_MP4_H264_800Kb_medium_360p25_high422_AAC_92.json
	MMS_MP4_H264_800Kb_veryfast_360p25_high422_AAC_92.json
	MMS_MP4_H264_800Kb_veryfast_360p25_high422_AAC_92.json
	MMS_TS_H264_800Kb_veryfast_360p25_high422_AAC_92.json
	MMS_HLS_H264_800Kb_veryfast_360p25_high422_AAC_92.json
	MMS_DASH_H264_800Kb_veryfast_360p25_high422_AAC_92.json
	MMS_MP4_H264_800Kb_veryslow_360p25_high422_AAC_92.json
	MMS_MP4_AV1_2000Kb_ConstrainedQuality_360p_AAC_92.json
	MMS_MP4_AV1_0Kb_ConstantQuality_360p_AAC_92.json
	MMS_MP4_AV1_2000Kb_AverageBitrate_360p0_AAC_92.json
	MMS_MP4_H264_2000Kb_veryfast_720p50_high422_AAC_92.json
	MMS_MP4_H264_2000Kb_veryfast_720p25_high422_AAC_92.json
	MMS_MP4_H264_4000Kb_veryfast_720p25_high422_AAC_192.json
	MMS_MP4_H264_6000Kb_veryfast_720p25_high422_AAC_192.json
	MMS_MP4_H264_2000Kb_medium_720p50_high422_AAC_92.json
	MMS_MP4_H264_2000Kb_medium_720p25_Main_AAC_92.json
	MMS_MP4_H264_1500Kb_medium_720p25_Main_AAC_92.json
	MMS_MP4_H264_1000Kb_medium_720p25_Main_AAC_92.json
	MMS_MP4_H264_2500Kb_medium_720p25_high422_AAC_92.json
	MMS_MP4_H264_2500Kb_medium_720p25_high422_AAC_160.json
	MMS_HLS_H264_3000Kb_medium_720p30_high422_AAC_160.json
	MMS_MP4_H264_2500Kb_medium_1080p25_Main_AAC_160.json
	MMS_MTS_H264_2500Kb_medium_1080p25_Main_AAC_160.json
	MMS_MTS_H264_2500Kb_medium_1080p25_high422_AAC_160.json
	MMS_TS_H264_2500Kb_medium_720p25_high422_AAC_160.json
	MMS_AVI_RAWVIDEO_2500Kb_1080p25_PCL_S32LE_160.json
	MMS_MP4_H264_1500Kb_veryslow_720p25_high422_AAC_160.json
	MMS_HLS_H264_1500Kb_veryslow_720p25_high422_AAC_160.json
	MMS_HLS_H264_1500Kb_medium_720p25_high422_AAC_160.json
	MMS_HLS_H264_2500Kb_medium_720p25_high422_AAC_160.json
	MMS_DASH_H264_1500Kb_veryslow_720p25_high422_AAC_160.json
	MMS_MP4_H264_6000Kb_veryslow_720p50_high422_AAC_160.json
	MMS_MP4_H264_6000Kb_veryslow_720p25_high422_AAC_160.json
	MMS_MP4_H264_5000Kb_veryfast_1080p50_high422_AAC_128.json
)

install (FILES ${PROFILEs} DESTINATION predefinedEncodingProfiles/video)

