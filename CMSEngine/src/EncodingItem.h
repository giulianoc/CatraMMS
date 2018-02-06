
#ifndef EncodingItem_h
#define EncodingItem_h

#include <string>
#include "CMSEngineDBFacade.h"

using namespace std;

enum EncodingJobStatus
{
    Free,
    ToBeRun,
    Running
};

struct EncodingItem
{
    long long                               _encodingJobKey;
    long long                               _ingestionJobKey;
    unsigned long                           _cmsPartitionNumber;
    string                                  _fileName;
    string                                  _relativePath;
    shared_ptr<Customer>                    _customer;
    long long                               _mediaItemKey;
    long long                               _physicalPathKey;
    CMSEngineDBFacade::ContentType          _contentType;
    unsigned long                           _encodingPriority;
    string                                  _ftpIPAddress;
    string                                  _ftpPort;
    string                                  _ftpUser;
    string                                  _ftpPassword;
    long long                               _encodingProfileKey;
    CMSEngineDBFacade::EncodingTechnology   _encodingProfileTechnology;
} ;

#endif

