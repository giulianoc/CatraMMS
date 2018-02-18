/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RegisterCustomer.h
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#ifndef RegisterCustomer_h
#define RegisterCustomer_h

#include "APICommon.h"

class RegisterCustomer: public APICommon {
public:
    RegisterCustomer();
    virtual ~RegisterCustomer();
    bool response();
private:

};

#endif /* POSTCUSTOMER_H */

