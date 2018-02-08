
#include "CheckEncodingTimes.h"
#include "catralibraries/Event.h"


CheckEncodingTimes:: CheckEncodingTimes (unsigned long ulPeriodInMilliSecs,
	shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger): 
    Times2 (ulPeriodInMilliSecs, CMSENGINE_CHECKENCODINGTIMES_CLASSNAME)

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

    shared_ptr<Event>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event>(CMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODINGEVENT);

    event->setSource(CMSENGINE_CHECKENCODINGTIMES_SOURCE);
    event->setDestination(CMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    _multiEventsSet->addEvent(event);
    
    _logger->info("addEvent: EVENT_TYPE ({}, {}, {})", 
            "CMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODING", 
            event->getEventKey().first, 
            event->getEventKey().second);
}
