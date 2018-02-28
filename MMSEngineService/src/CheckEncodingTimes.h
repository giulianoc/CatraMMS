
#ifndef CheckEncodingTimes_h
#define CheckEncodingTimes_h

#include <memory>
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "catralibraries/Times2.h"

#define MMSENGINE_CHECKENCODINGTIMES_CLASSNAME		"CheckEncodingTimes"
#define MMSENGINE_CHECKENCODINGTIMES_SOURCE		"CheckEncodingTimes"

#define MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODINGEVENT	3
#define MMSENGINEPROCESSORNAME                          "MMSEngineProcessor"


#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename(__FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

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

