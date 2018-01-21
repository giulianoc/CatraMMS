
#include "CheckIngestionTimes.h"
#include "catralibraries/Event.h"


CheckIngestionTimes:: CheckIngestionTimes (unsigned long ulPeriodInMilliSecs,
	shared_ptr<MultiEventsSet> multiEventsSet): 
    Times2 (ulPeriodInMilliSecs, CMSENGINE_CHECKINGESTIONTIMES_CLASSNAME)

{
    _multiEventsSet     = multiEventsSet;
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

    shared_ptr<Event>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event>(CMSENGINE_EVENT_TYPEIDENTIFIER);

    event->setSource(CMSENGINE_CHECKINGESTIONTIMES_SOURCE);
    event->setDestination(CMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    cout << "addEvent: EVENT_TYPE (" << event->getEventKey().first << ", " << event->getEventKey().second << ")" << endl;
    _multiEventsSet->addEvent(event);                    
}
