
#ifndef CheckIngestionTimes_h
#define CheckIngestionTimes_h

#include <memory>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "catralibraries/Times2.h"

#define MMSENGINE_CHECKINGESTIONTIMES_CLASSNAME		"CheckIngestionTimes"
#define MMSENGINE_CHECKINGESTIONTIMES_SOURCE		"CheckIngestionTimes"

#define MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTIONEVENT	1
#define MMSENGINEPROCESSORNAME                          "MMSEngineProcessor"


#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename(__FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

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

