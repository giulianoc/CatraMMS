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
    string                  _ftpDirectoryMediaSourceFileName;
    string                  _mediaSourceFileName;
    string                  _relativePath;
    CMSEngineDBFacade::ContentType   _contentType;
    
    int64_t                 _ingestionJobKey;
    shared_ptr<Customer>    _customer;

    string                  _ftpDirectoryWorkingMetadataPathName;
    string                  _metadataFileName;
    Json::Value             _metadataRoot;
    
public:
    void setFTPDirectoryWorkingMetadataPathName(string ftpDirectoryWorkingMetadataPathName)
    {
        _ftpDirectoryWorkingMetadataPathName   = ftpDirectoryWorkingMetadataPathName;
    }

    string getFTPDirectoryWorkingMetadataPathName()
    {
        return _ftpDirectoryWorkingMetadataPathName;
    }

    void setFTPDirectoryMediaSourceFileName(string ftpDirectoryMediaSourceFileName)
    {
        _ftpDirectoryMediaSourceFileName   = ftpDirectoryMediaSourceFileName;
    }

    string getFTPDirectoryMediaSourceFileName()
    {
        return _ftpDirectoryMediaSourceFileName;
    }

    void setMediaSourceFileName(string mediaSourceFileName)
    {
        _mediaSourceFileName = mediaSourceFileName;
    }
    
    string getMediaSourceFileName()
    {
        return _mediaSourceFileName;
    }
    
    void setIngestionJobKey(int64_t ingestionJobKey)
    {
        _ingestionJobKey   = ingestionJobKey;
    }    
    int64_t getIngestionJobKey()
    {
        return _ingestionJobKey;
    }
    
    void setContentType(CMSEngineDBFacade::ContentType contentType)
    {
        _contentType   = contentType;
    }    
    CMSEngineDBFacade::ContentType getContentType()
    {
        return _contentType;
    }
    
    void setMetadataFileName(string metadataFileName)
    {
        _metadataFileName   = metadataFileName;
    }

    string getMetadataFileName()
    {
        return _metadataFileName;
    }
    
    void setRelativePath(string relativePath)
    {
        _relativePath   = relativePath;
    }

    string getRelativePath()
    {
        return _relativePath;
    }

    void setCustomer(shared_ptr<Customer> customer)
    {
        _customer   = customer;
    }

    shared_ptr<Customer> getCustomer()
    {
        return _customer;
    }

    void setMetadataRoot(Json::Value metadataRoot)
    {
        _metadataRoot   = metadataRoot;
    }

    Json::Value getMetadataRoot()
    {
        return _metadataRoot;
    }
};

#endif
