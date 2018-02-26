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
    _binaryBufferLength     = 1024 * 10;
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
        buffer = new char [_binaryBufferLength];

        unsigned long currentRead;
        unsigned long totalRead = 0;
        if (contentLength == -1)
        {
            // we do not have the content-length header, we will read all what we receive
            do
            {
                cin.read(buffer, _binaryBufferLength);

                currentRead = cin.gcount();

                totalRead   += currentRead;

                binaryFileStream.write(buffer, currentRead); 
            }
            while (currentRead == _binaryBufferLength);
        }
        else
        {
            // we have the content-length and we will use it to read the binary
            
            unsigned long bytesToBeRead;
            while (totalRead < contentLength)
            {
                if (contentLength - totalRead >= _binaryBufferLength)
                    bytesToBeRead = _binaryBufferLength;
                else
                    bytesToBeRead = contentLength - totalRead;

                cin.read(buffer, bytesToBeRead);
                currentRead = cin.gcount();
                if (currentRead != bytesToBeRead)
                {
                    // this should never happen because it will be against the content-length
                    string errorMessage = string("Error reading the binary")
                        + ", contentLength: " + to_string(contentLength)
                        + ", totalRead: " + to_string(totalRead)
                        + ", bytesToBeRead: " + to_string(bytesToBeRead)
                        + ", currentRead: " + to_string(currentRead)
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(400, errorMessage);

                    throw runtime_error(errorMessage);            
                }

                totalRead   += currentRead;

                binaryFileStream.write(buffer, currentRead); 
            }
        }

        binaryFileStream.close();
        
        delete buffer;

        _logger->info(__FILEREF__ + "Binary read"
            + ", contentLength: " + to_string(contentLength)
            + ", totalRead: " + to_string(totalRead)
        );

        /*
        {
            // Chew up any remaining stdin - this shouldn't be necessary
            // but is because mod_fastcgi doesn't handle it correctly.

            // ignore() doesn't set the eof bit in some versions of glibc++
            // so use gcount() instead of eof()...
            do 
                cin.ignore(bufferLength); 
            while (cin.gcount() == bufferLength);
        }    
        */
        
        string responseBody = string("{ ")
            + "\"sourceFileName\": \"" + sourceFileName + "\" "
            + "\"contentLength\": " + to_string(contentLength) + " "
            + "\"writtenBytes\": " + to_string(totalRead) + " "
            + "}";
        sendSuccess(201, responseBody);
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "getBinaryAndResponse failed");
        
        if (buffer != nullptr)
            delete [] buffer;
            
        throw e;
    }    
}
