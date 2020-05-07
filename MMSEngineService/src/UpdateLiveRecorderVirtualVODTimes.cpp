
#include "UpdateLiveRecorderVirtualVODTimes.h"
#include "catralibraries/Event2.h"


UpdateLiveRecorderVirtualVODTimes:: UpdateLiveRecorderVirtualVODTimes (unsigned long ulPeriodInMilliSecs,
	shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger): 
    Times2 (ulPeriodInMilliSecs, MMSENGINE_UPDATELIVERECORDERVIRTUALVODTIMES_CLASSNAME)

{
    _multiEventsSet     = multiEventsSet;
    _logger             = logger;
}

UpdateLiveRecorderVirtualVODTimes::~UpdateLiveRecorderVirtualVODTimes (void)
{
    
}

void UpdateLiveRecorderVirtualVODTimes:: handleTimeOut (void)
{

    lock_guard<mutex>   locker(_mtTimesMutex);

    if (_schTimesStatus != SCHTIMES_STARTED)
    {
        return;
    }

    shared_ptr<Event2>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event2>(MMSENGINE_EVENTTYPEIDENTIFIER_UPDATELIVERECORDERVIRTUALVOD);

    event->setSource(MMSENGINE_UPDATELIVERECORDERVIRTUALVODTIMES_SOURCE);
    event->setDestination(MMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    _multiEventsSet->addEvent(event);
    
    _logger->debug(__FILEREF__ + "addEvent: EVENT_TYPE" 
            + ", MMSENGINE_EVENTTYPEIDENTIFIER_UPDATELIVERECORDERVIRTUALVOD"
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second)
    );
}
