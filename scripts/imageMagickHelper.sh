#!/bin/bash

MMS_PATH=/opt/mms

#used by ImageMagick to look for the configuration files
export MAGICK_CONFIGURE_PATH=$MMS_PATH/ImageMagick/etc/ImageMagick-7

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$MMS_PATH/ImageMagick/lib
export PATH=$PATH:$MMS_PATH/ImageMagick/bin


#$MMS_PATH/ImageMagick/bin/convert LogoRSI.png LogoRSI.jpg


