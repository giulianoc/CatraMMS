
#ifndef RetentionTimes_h
#define RetentionTimes_h

#include <memory>
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "catralibraries/Times2.h"

#define MMSENGINE_RETENTIONTIMES_CLASSNAME      "RetentionTimes"
#define MMSENGINE_RETENTIONTIMES_SOURCE		"RetentionTimes"

#define MMSENGINE_EVENTTYPEIDENTIFIER_CONTENTRETENTIONEVENT	4
#define MMSENGINEPROCESSORNAME                          "MMSEngineProcessor"


#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename(__FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

class RetentionTimes: public Times2
{
protected:
    shared_ptr<MultiEventsSet>              _multiEventsSet;
    shared_ptr<spdlog::logger>              _logger;

public:
    RetentionTimes (string contentRetentionTimesSchedule,
        shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<spdlog::logger> logger);

    virtual ~RetentionTimes (void);

    virtual void handleTimeOut (void);

} ;

#endif

