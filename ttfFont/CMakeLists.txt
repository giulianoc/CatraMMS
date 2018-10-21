
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

SET (TTFFONTs
	CAC-Champagne/cac_champagne.ttf
	dancing-script-ot/DancingScript-Regular.otf
	open-sans/OpenSans-BoldItalic.ttf
	open-sans/OpenSans-ExtraBoldItalic.ttf
	open-sans/OpenSans-Italic.ttf
	open-sans/OpenSans-Light.ttf
	open-sans/OpenSans-SemiboldItalic.ttf
	open-sans/OpenSans-Bold.ttf
	open-sans/OpenSans-ExtraBold.ttf
	open-sans/OpenSans-LightItalic.ttf
	open-sans/OpenSans-Regular.ttf
	open-sans/OpenSans-Semibold.ttf
	pacifico/Pacifico.ttf
	sofia/Sofia-Regular.otf
	Windsong/Windsong.ttf
)

install (FILES ${TTFFONTs} DESTINATION ttfFont)

