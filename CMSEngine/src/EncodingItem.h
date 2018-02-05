
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
    long long                               _customerKey;
    string                                  _customerName;
    string                                  _customerDirectoryName;
    long long                               _mediaItemKey;
    long long                               _physicalPathKey;
    CMSEngineDBFacade::ContentType          _contentType;
    unsigned long                           _encodingPriority;
    string                                  _ftpIPAddress;
    string                                  _ftpPort;
    string                                  _ftpUser;
    string                                  _ftpPassword;
    long long                               _encodingProfileKey;
    int                                     _encodingProfileTechnology;


    EncodingItem (void)
    {
        _encodingJobKey             = 0;
        _ingestionJobKey            = 0;
        _cmsPartitionNumber         = 9999999;	// undefined
        _customerKey                = 0;
        _mediaItemKey               = 0;
        _physicalPathKey            = 0;
        _contentType                = CMSEngineDBFacade::ContentType::Video;
        _encodingProfileKey         = 0;
        _encodingProfileTechnology  = -1;
    } ;

    ~EncodingItem (void)
    {
    } ;
} ;

#endif

