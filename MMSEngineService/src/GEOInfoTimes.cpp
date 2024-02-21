
#include "GEOInfoTimes.h"
#include "catralibraries/Event2.h"


GEOInfoTimes:: GEOInfoTimes (string geoInfoTimesSchedule,
	shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<spdlog::logger> logger): 
    Times2 (geoInfoTimesSchedule, MMSENGINE_GEOINFOTIMES_CLASSNAME)

{
    _multiEventsSet     = multiEventsSet;
    _logger             = logger;
}

GEOInfoTimes::~GEOInfoTimes (void)
{
    
}

void GEOInfoTimes:: handleTimeOut (void)
{

    lock_guard<mutex>   locker(_mtTimesMutex);

    if (_schTimesStatus != SCHTIMES_STARTED)
    {
        return;
    }

    shared_ptr<Event2>    event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event2>(
            MMSENGINE_EVENTTYPEIDENTIFIER_GEOINFOEVENT);

    event->setSource(MMSENGINE_GEOINFOTIMES_SOURCE);
    event->setDestination(MMSENGINEPROCESSORNAME);
    event->setExpirationTimePoint(chrono::system_clock::now());

    _multiEventsSet->addEvent(event);
    
    _logger->debug(__FILEREF__ + "addEvent: EVENT_TYPE" 
            + ", MMSENGINE_EVENTTYPEIDENTIFIER_RETENTION"
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second)
    );
}
