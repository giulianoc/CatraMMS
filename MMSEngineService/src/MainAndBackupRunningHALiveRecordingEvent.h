
#ifndef MainAndBackupRunningHALiveRecordingEvent_h
#define MainAndBackupRunningHALiveRecordingEvent_h

#include <memory>
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "catralibraries/Times2.h"

#define MMSENGINE_MAINANDBACKUPRUNNINGHALIVERECORDING_CLASSNAME		"MainAndBackupRunningHALiveRecordingEvent"
#define MMSENGINE_MAINANDBACKUPRUNNINGHALIVERECORDING_SOURCE		"MainAndBackupRunningHALiveRecordingEvent"

#define MMSENGINE_EVENTTYPEIDENTIFIER_MAINANDBACKUPRUNNINGHALIVERECORDINGEVENT	6
#define MMSENGINEPROCESSORNAME                          "MMSEngineProcessor"


#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename(__FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

class MainAndBackupRunningHALiveRecordingEvent: public Times2
{
protected:
    shared_ptr<MultiEventsSet>              _multiEventsSet;
    shared_ptr<spdlog::logger>              _logger;

public:
    MainAndBackupRunningHALiveRecordingEvent (string timesSchedule,
        shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<spdlog::logger> logger);

    virtual ~MainAndBackupRunningHALiveRecordingEvent (void);

    virtual void handleTimeOut (void);

} ;

#endif

