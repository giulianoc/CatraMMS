
#ifndef CheckIngestionTimes_h
#define CheckIngestionTimes_h

#include <memory>
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "catralibraries/Times2.h"

#define CMSENGINE_CHECKINGESTIONTIMES_CLASSNAME		"CheckIngestionTimes"
#define CMSENGINE_CHECKINGESTIONTIMES_SOURCE		"CheckIngestionTimes"

#define CMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION	1
#define CMSENGINEPROCESSORNAME                          "CMSEngineProcessor"



class CheckIngestionTimes: public Times2
{
protected:
    shared_ptr<MultiEventsSet>              _multiEventsSet;
    shared_ptr<spdlog::logger>              _logger;

public:
    CheckIngestionTimes (unsigned long ulPeriodInMilliSecs,
        shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<spdlog::logger> logger);

    virtual ~CheckIngestionTimes (void);

    virtual void handleTimeOut (void);

} ;

#endif

