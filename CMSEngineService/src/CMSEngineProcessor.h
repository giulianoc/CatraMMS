
#ifndef CMSEngineProcessor_h
#define CMSEngineProcessor_h

#include <string>
// #define SPDLOG_DEBUG_ON
// #define SPDLOG_TRACE_ON
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "CMSEngineDBFacade.h"
#include "CMSStorage.h"

#define CMSENGINEPROCESSORNAME    "CMSEngineProcessor"

class CMSEngineProcessor
{
private:
    shared_ptr<spdlog::logger>          _logger;
    shared_ptr<CMSEngineDBFacade>       _cmsEngineDBFacade;
    shared_ptr<CMSStorage>           _cmsStorage;
    
    unsigned long                       _ulIngestionLastCustomerIndex;
    unsigned long                       _ulMaxIngestionsNumberPerCustomerEachIngestionPeriod;

    void handleCheckIngestionEvent();

public:
    CMSEngineProcessor(
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
            shared_ptr<CMSStorage> cmsStorage);
    
    ~CMSEngineProcessor();
    
    void operator()(shared_ptr<MultiEventsSet> multiEventsSet);
} ;

#endif

