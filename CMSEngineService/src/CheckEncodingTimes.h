
#ifndef CheckEncodingTimes_h
#define CheckEncodingTimes_h

#include <memory>
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "catralibraries/Times2.h"

#define CMSENGINE_CHECKENCODINGTIMES_CLASSNAME		"CheckEncodingTimes"
#define CMSENGINE_CHECKENCODINGTIMES_SOURCE		"CheckEncodingTimes"

#define CMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODINGEVENT	3
#define CMSENGINEPROCESSORNAME                          "CMSEngineProcessor"



class CheckEncodingTimes: public Times2
{
protected:
    shared_ptr<MultiEventsSet>              _multiEventsSet;
    shared_ptr<spdlog::logger>              _logger;

public:
    CheckEncodingTimes (unsigned long ulPeriodInMilliSecs,
        shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<spdlog::logger> logger);

    virtual ~CheckEncodingTimes (void);

    virtual void handleTimeOut (void);

} ;

#endif

