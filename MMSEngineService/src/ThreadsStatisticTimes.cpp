
#include "ThreadsStatisticTimes.h"
#include "catralibraries/Event2.h"


ThreadsStatisticTimes:: ThreadsStatisticTimes (unsigned long ulPeriodInMilliSecs,
	shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger): 
    Times2 (ulPeriodInMilliSecs, MMSENGINE_THREADSSTATISTICTIMES_CLASSNAME)

{
    _multiEventsSet     = multiEventsSet;
    _logger             = logger;
}

ThreadsStatisticTimes::~ThreadsStatisticTimes (void)
{
    
}

void ThreadsStatisticTimes:: handleTimeOut (void)
{

    lock_guard<mutex>   locker(_mtTimesMutex);

    if (_schTimesStatus != SCHTIMES_STARTED)
    {
        return;
    }

    shared_ptr<Event2>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event2>(
		MMSENGINE_EVENTTYPEIDENTIFIER_THREADSSTATISTICEVENT);

    event->setSource(MMSENGINE_THREADSSTATISTICTIMES_SOURCE);
    event->setDestination(MMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    _multiEventsSet->addEvent(event);
    
    _logger->debug(__FILEREF__ + "addEvent: EVENT_TYPE" 
            + ", MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION"
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second)
    );
}
