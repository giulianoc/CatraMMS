
#ifndef CMSEngineProcessor_h
#define CMSEngineProcessor_h

#include <string>
// #define SPDLOG_DEBUG_ON
// #define SPDLOG_TRACE_ON
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"

#define CMSENGINEPROCESSORNAME    "CMSEngineProcessor"

class CMSEngineProcessor
{
private:
    shared_ptr<spdlog::logger>          _logger;
    
    void handleCheckIngestionEvent();

public:
    CMSEngineProcessor(shared_ptr<spdlog::logger> logger);
    
    ~CMSEngineProcessor();
    
    void operator()(shared_ptr<MultiEventsSet> multiEventsSet);
} ;

#endif

