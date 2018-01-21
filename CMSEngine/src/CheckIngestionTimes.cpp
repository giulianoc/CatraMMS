
#include "CheckIngestionTimes.h"
#include "catralibraries/Event.h"


CheckIngestionTimes:: CheckIngestionTimes (unsigned long ulPeriodInMilliSecs,
	shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger): 
    Times2 (ulPeriodInMilliSecs, CMSENGINE_CHECKINGESTIONTIMES_CLASSNAME)

{
    _multiEventsSet     = multiEventsSet;
    _logger             = logger;
}

CheckIngestionTimes::~CheckIngestionTimes (void)
{
    
}

void CheckIngestionTimes:: handleTimeOut (void)
{

    lock_guard<mutex>   locker(_mtTimesMutex);

    if (_schTimesStatus != SCHTIMES_STARTED)
    {
        return;
    }

    shared_ptr<Event>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event>(CMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION);

    event->setSource(CMSENGINE_CHECKINGESTIONTIMES_SOURCE);
    event->setDestination(CMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    _multiEventsSet->addEvent(event);
    
    _logger->info("addEvent: EVENT_TYPE ({}, {})", event->getEventKey().first, event->getEventKey().second);
}
