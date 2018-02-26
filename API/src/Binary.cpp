/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Binary.cpp
 * Author: giuliano
 * 
 * Created on February 18, 2018, 1:27 AM
 */

#include <fstream>
#include "Binary.h"

int main(int argc, char** argv) 
{
    Binary binary;
    
    return binary.manageBinaryRequest();
}

Binary::Binary(): APICommon() 
{
}

Binary::~Binary() {
}

void Binary::manageRequestAndResponse(
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength,
        string requestBody
)
{
    _logger->error(__FILEREF__ + "Binary application is able to manage ONLY Binary requests");
    
    string errorMessage = string("Internal server error");
    _logger->error(__FILEREF__ + errorMessage);

    sendError(500, errorMessage);

    throw runtime_error(errorMessage);
}

void Binary::getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength
)
{
    unsigned long bufferLength = 1024;
    char* buffer = nullptr;

    try
    {        
        auto sourceFileNameIt = queryParameters.find("sourceFileName");
        if (sourceFileNameIt == queryParameters.end())
        {
            string errorMessage = string("'sourceFileName' query parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        string sourceFileName = sourceFileNameIt->second;

        shared_ptr<Customer> customer = get<0>(customerAndFlags);
        string customerFTPBinaryPathName = _cmsStorage->getCustomerFTPRepository(customer);
        customerFTPBinaryPathName
                .append("/")
                .append(sourceFileName)
                ;
        
        _logger->info(__FILEREF__ + "Customer FTP Binary path name"
            + ", customerFTPBinaryPathName: " + customerFTPBinaryPathName
        );

        ofstream binaryFileStream(customerFTPBinaryPathName, ofstream::binary);
        buffer = new char [bufferLength];

        unsigned long currentRead;
        do
        {
            cin.read(buffer, bufferLength);

            currentRead = cin.gcount();

            binaryFileStream.write(buffer, currentRead); 
        }
        while (currentRead == bufferLength);

        binaryFileStream.close();
        
        delete buffer;

        {
            // Chew up any remaining stdin - this shouldn't be necessary
            // but is because mod_fastcgi doesn't handle it correctly.

            // ignore() doesn't set the eof bit in some versions of glibc++
            // so use gcount() instead of eof()...
            do 
                cin.ignore(bufferLength); 
            while (cin.gcount() == bufferLength);
        }    
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "getBinaryAndResponse failed");
        
        if (buffer != nullptr)
            delete [] buffer;
            
        throw e;
    }    
}
