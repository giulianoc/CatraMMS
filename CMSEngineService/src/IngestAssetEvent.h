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
/*
#include <thread>
#include <chrono>
#include <memory>
#include <vector>

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

*/
        
using namespace std;

#define CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT	2


class IngestAssetEvent: public Event {
private:
    string                  _ftpDirectoryWorkingEntryPathName;
    int64_t                 _ingestionJobKey;
    shared_ptr<Customer>    _customer;
    string                  _fileName;
    
public:
    void setFTPDirectoryWorkingEntryPathName(string ftpDirectoryWorkingEntryPathName)
    {
        _ftpDirectoryWorkingEntryPathName   = ftpDirectoryWorkingEntryPathName;
    }

    string getFTPDirectoryWorkingEntryPathName()
    {
        return _ftpDirectoryWorkingEntryPathName;
    }

    void setIngestionJobKey(int64_t ingestionJobKey)
    {
        _ingestionJobKey   = ingestionJobKey;
    }
    
    int64_t getIngestionJobKey()
    {
        return _ingestionJobKey;
    }
    
    void setFileName(string fileName)
    {
        _fileName   = fileName;
    }

    string getFileName()
    {
        return _fileName;
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
