
#include "CheckEncodingTimes.h"
#include "catralibraries/Event2.h"


CheckEncodingTimes:: CheckEncodingTimes (unsigned long ulPeriodInMilliSecs,
	shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger): 
    Times2 (ulPeriodInMilliSecs, MMSENGINE_CHECKENCODINGTIMES_CLASSNAME)

{
    _multiEventsSet     = multiEventsSet;
    _logger             = logger;
}

CheckEncodingTimes::~CheckEncodingTimes (void)
{
    
}

void CheckEncodingTimes:: handleTimeOut (void)
{

    lock_guard<mutex>   locker(_mtTimesMutex);

    if (_schTimesStatus != SCHTIMES_STARTED)
    {
        return;
    }

    shared_ptr<Event2>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event2>(MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODINGEVENT);

    event->setSource(MMSENGINE_CHECKENCODINGTIMES_SOURCE);
    event->setDestination(MMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    _multiEventsSet->addEvent(event);
    
    _logger->debug(__FILEREF__ + "addEvent: EVENT_TYPE" 
            + ", MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODING"
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second)
    );
}
