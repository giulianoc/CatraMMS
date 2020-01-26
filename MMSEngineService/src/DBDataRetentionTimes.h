
#ifndef DBDataRetentionTimes_h
#define DBDataRetentionTimes_h

#include <memory>
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "catralibraries/Times2.h"

#define MMSENGINE_DBDATARETENTIONTIMES_CLASSNAME      "DBDataRetentionTimes"
#define MMSENGINE_DBDATARETENTIONTIMES_SOURCE		"DBDataRetentionTimes"

#define MMSENGINE_EVENTTYPEIDENTIFIER_DBDATARETENTIONEVENT	7
#define MMSENGINEPROCESSORNAME                          "MMSEngineProcessor"


#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename(__FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

class DBDataRetentionTimes: public Times2
{
protected:
    shared_ptr<MultiEventsSet>              _multiEventsSet;
    shared_ptr<spdlog::logger>              _logger;

public:
    DBDataRetentionTimes (string dbDataRetentionTimesSchedule,
        shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<spdlog::logger> logger);

    virtual ~DBDataRetentionTimes (void);

    virtual void handleTimeOut (void);

} ;

#endif

