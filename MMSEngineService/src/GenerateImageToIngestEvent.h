/*
 Copyright (C) Giuliano Catrambone (giuliano.catrambone@catrasoftware.it)

 This program is free software; you can redistribute it and/or 
 modify it under the terms of the GNU General Public License 
 as published by the Free Software Foundation; either 
 version 2 of the License, or (at your option) any later 
 version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 Commercial use other than under the terms of the GNU General Public
 License is allowed only after express negotiation of conditions
 with the authors.
*/

#ifndef GenerateImageToIngestEvent_h
#define GenerateImageToIngestEvent_h

#include <iostream>
#include "catralibraries/Event2.h"
        
using namespace std;

#define MMSENGINE_EVENTTYPEIDENTIFIER_GENERATEIMAGETOINGESTEVENT	4


class GenerateImageToIngestEvent: public Event2 {
private:
    string                  _mmsVideoPathName;
    shared_ptr<Customer>    _customer;
    string                  _imageFileName;
    string                  _imageTitle;

    double                  _timePositionInSeconds;
    string                  _encodingProfilesSet;
    int                     _sourceImageWidth;    
    int                     _sourceImageHeight;    

public:
    string getCmsVideoPathName() {
        return _mmsVideoPathName;
    }
    void setCmsVideoPathName(string mmsVideoPathName) {
        _mmsVideoPathName = mmsVideoPathName;
    }

    shared_ptr<Customer> getCustomer() {
        return _customer;
    }
    void setCustomer(shared_ptr<Customer> customer) {
        _customer = customer;
    }

    string getImageFileName() {
        return _imageFileName;
    }
    void setImageFileName(string imageFileName) {
        _imageFileName = imageFileName;
    }

    string getImageTitle() {
        return _imageTitle;
    }
    void setImageTitle(string imageTitle) {
        _imageTitle = imageTitle;
    }

    double getTimePositionInSeconds() {
        return _timePositionInSeconds;
    }
    void setTimePositionInSeconds(double timePositionInSeconds) {
        _timePositionInSeconds = timePositionInSeconds;
    }

    string getEncodingProfilesSet() {
        return _encodingProfilesSet;
    }
    void setEncodingProfilesSet(string encodingProfilesSet) {
        _encodingProfilesSet = encodingProfilesSet;
    }

    int getSourceImageWidth() {
        return _sourceImageWidth;
    }
    void setSourceImageWidth(int sourceImageWidth) {
        _sourceImageWidth = sourceImageWidth;
    }

    int getSourceImageHeight() {
        return _sourceImageHeight;
    }
    void setSourceImageHeight(int sourceImageHeight) {
        _sourceImageHeight = sourceImageHeight;
    }
};

#endif
