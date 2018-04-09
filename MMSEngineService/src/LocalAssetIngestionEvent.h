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

#ifndef LocalAssetIngestionEvent_h
#define LocalAssetIngestionEvent_h

#include <iostream>
#include "catralibraries/Event2.h"
#include "MMSEngineDBFacade.h"
        
using namespace std;

#define MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT	2


class LocalAssetIngestionEvent: public Event2 {
private:    
    int64_t                 _ingestionJobKey;
    shared_ptr<Customer>    _customer;
    MMSEngineDBFacade::IngestionType _ingestionType;

    string                  _metadataContent;
    string                  _sourceFileName;
    bool                    _customerIngestionBinarySourceFileNameToBeUsed;

public:
    void setCustomerIngestionBinarySourceFileNameToBeUsed(bool customerIngestionBinarySourceFileNameToBeUsed)
    {
        _customerIngestionBinarySourceFileNameToBeUsed   = customerIngestionBinarySourceFileNameToBeUsed;
    }
    bool getCustomerIngestionBinarySourceFileNameToBeUsed()
    {
        return _customerIngestionBinarySourceFileNameToBeUsed;
    }

    void setIngestionType(MMSEngineDBFacade::IngestionType ingestionType)
    {
        _ingestionType   = ingestionType;
    }
    MMSEngineDBFacade::IngestionType getIngestionType()
    {
        return _ingestionType;
    }

    void setMetadataContent(string metadataContent)
    {
        _metadataContent   = metadataContent;
    }
    string getMetadataContent()
    {
        return _metadataContent;
    }

    void setSourceFileName(string sourceFileName)
    {
        _sourceFileName   = sourceFileName;
    }
    string getSourceFileName()
    {
        return _sourceFileName;
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
