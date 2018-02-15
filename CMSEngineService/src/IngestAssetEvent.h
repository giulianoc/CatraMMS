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

#ifndef IngestAssetEvent_h
#define IngestAssetEvent_h

#include <iostream>
#include "catralibraries/Event.h"
#include "CMSEngineDBFacade.h"
        
using namespace std;

#define CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT	2


class IngestAssetEvent: public Event {
private:    
    /*
    tuple<bool, string, string, string, int>    _mediaSourceDetails;
    string                  _relativePath;
    CMSEngineDBFacade::ContentType   _contentType;
    */
    int64_t                 _ingestionJobKey;
    shared_ptr<Customer>    _customer;

    string                  _metadataFileName;
    string                  _ftpWorkingMetadataPathName;
    string                  _mediaSourceFileName;
    string                  _ftpMediaSourcePathName;

public:
    void setFTPWorkingMetadataPathName(string ftpWorkingMetadataPathName)
    {
        _ftpWorkingMetadataPathName   = ftpWorkingMetadataPathName;
    }
    string getFTPWorkingMetadataPathName()
    {
        return _ftpWorkingMetadataPathName;
    }

    void setMediaSourceFileName(string mediaSourceFileName)
    {
        _mediaSourceFileName   = mediaSourceFileName;
    }
    string getMediaSourceFileName()
    {
        return _mediaSourceFileName;
    }

    void setFTPMediaSourcePathName(string ftpMediaSourcePathName)
    {
        _ftpMediaSourcePathName   = ftpMediaSourcePathName;
    }
    string getFTPMediaSourcePathName()
    {
        return _ftpMediaSourcePathName;
    }

    void setMetadataFileName(string metadataFileName)
    {
        _metadataFileName   = metadataFileName;
    }
    string getMetadataFileName()
    {
        return _metadataFileName;
    }

    void setIngestionJobKey(int64_t ingestionJobKey)
    {
        _ingestionJobKey   = ingestionJobKey;
    }    
    int64_t getIngestionJobKey()
    {
        return _ingestionJobKey;
    }

    void setCustomer(shared_ptr<Customer> customer)
    {
        _customer   = customer;
    }

    shared_ptr<Customer> getCustomer()
    {
        return _customer;
    }
};

#endif
