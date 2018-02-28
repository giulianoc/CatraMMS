
#include "CheckIngestionTimes.h"
#include "catralibraries/Event.h"


CheckIngestionTimes:: CheckIngestionTimes (unsigned long ulPeriodInMilliSecs,
	shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger): 
    Times2 (ulPeriodInMilliSecs, MMSENGINE_CHECKINGESTIONTIMES_CLASSNAME)

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

    shared_ptr<Event>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event>(MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTIONEVENT);

    event->setSource(MMSENGINE_CHECKINGESTIONTIMES_SOURCE);
    event->setDestination(MMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    _multiEventsSet->addEvent(event);
    
    _logger->debug(__FILEREF__ + "addEvent: EVENT_TYPE" 
            + ", MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION"
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second)
    );
}
