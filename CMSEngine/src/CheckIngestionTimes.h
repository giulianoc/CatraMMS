
#ifndef CheckIngestionTimes_h
#define CheckIngestionTimes_h

#include <memory>
#include "catralibraries/MultiEventsSet.h"
#include "catralibraries/Times2.h"

#define CMSENGINE_CHECKINGESTIONTIMES_CLASSNAME		"CheckIngestionTimes"
#define CMSENGINE_CHECKINGESTIONTIMES_SOURCE		"CheckIngestionTimes"

#define CMSENGINE_EVENT_TYPEIDENTIFIER			1
#define CMSENGINEPROCESSORNAME                          "CMSEngineProcessor"

#define CMS_EVENT_CHECKINGESTION			10


class CheckIngestionTimes: public Times2
{
protected:
    shared_ptr<MultiEventsSet>              _multiEventsSet;

public:
    CheckIngestionTimes (unsigned long ulPeriodInMilliSecs,
        shared_ptr<MultiEventsSet> multiEventsSet);

    virtual ~CheckIngestionTimes (void);

    virtual void handleTimeOut (void);

} ;

#endif

