/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CMSEngine.h
 * Author: multi
 *
 * Created on January 30, 2018, 3:00 PM
 */

#ifndef CMSEngine_h
#define CMSEngine_h

#include <string>
#include "CMSEngineDBFacade.h"

using namespace std;

class CMSEngine {
private:
    shared_ptr<CMSEngineDBFacade> _cmsEngineDBFacade;
    
public:
    CMSEngine(shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade);
    CMSEngine(const CMSEngine& orig);
    virtual ~CMSEngine();
    
    void addCustomer(
        string sCreationDate, 
        string sDefaultUserExpirationDate,
	string customerName,
        string password,
	string street,
        string city,
        string state,
	string zip,
        string phone,
        string countryCode,
	string deliveryURL,
        long enabled,
	long maxEncodingPriority,
        long period,
	long maxIngestionsNumber,
        long maxStorageInGB,
	string languageCode
);

};

#endif

