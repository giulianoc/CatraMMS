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

set(PROFILEs
    MMS_PNG_1024_576.json
    MMS_JPG_W120.json
    MMS_JPG_W150_H-120.json
    MMS_JPG_W150_H-200.json
    MMS_JPG_W240.json
    MMS_JPG_W240_H-200.json
    MMS_JPG_W160.json
    MMS_JPG_W576.json
    MMS_JPG_W40.json
    MMS_JPG_W60.json
    MMS_JPG_H700.json
    MMS_JPG_W930.json
    MMS_PNG_W120.json
    MMS_PNG_W1280.json
    MMS_JPG_W1280.json
    MMS_JPG_W1920.json
    MMS_PNG_W240.json
    MMS_PNG_W40.json
    MMS_PNG_W60.json)

install(FILES ${PROFILEs} DESTINATION predefinedEncodingProfiles/image)
