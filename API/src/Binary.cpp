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
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength
)
{
    char* buffer = nullptr;

    try
    {
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("'ingestionJobKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        int64_t ingestionJobKey = stol(ingestionJobKeyIt->second);

        string sourceFileName;
        try
        {
            sourceFileName = _mmsEngineDBFacade->getSourceReferenceOfUploadingInProgress(ingestionJobKey);
        }
        catch(exception e)
        {
            string errorMessage = string("The 'ingestionJobKey' URI parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        
        shared_ptr<Customer> customer = get<0>(customerAndFlags);
        string customerFTPBinaryPathName = _mmsStorage->getCustomerFTPRepository(customer);
        customerFTPBinaryPathName
                .append("/")
                .append(sourceFileName)
                ;
        
        _logger->info(__FILEREF__ + "Customer FTP Binary path name"
            + ", customerFTPBinaryPathName: " + customerFTPBinaryPathName
        );
        
        if (requestMethod == "HEAD")
        {
            unsigned long fileSize = 0;
            try
            {
                if (FileIO::fileExisting(customerFTPBinaryPathName))
                {
                    bool inCaseOfLinkHasItToBeRead = false;
                    fileSize = FileIO::getFileSizeInBytes (
                        customerFTPBinaryPathName, inCaseOfLinkHasItToBeRead);
                }
            }
            catch(exception e)
            {
                string errorMessage = string("Error to retrieve the file size")
                    + ", sourceFileName: " + sourceFileName
                ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(500, errorMessage);

                throw runtime_error(errorMessage);            
            }

            sendHeadSuccess(200, fileSize);
        }
        else
        {
            chrono::system_clock::time_point uploadStartTime = chrono::system_clock::now();

            bool resume = false;
            {
                if (xCatraMMSResumeHeader != "")
                {
                    unsigned long fileSize = 0;
                    try
                    {
                        if (FileIO::fileExisting(customerFTPBinaryPathName))
                        {
                            bool inCaseOfLinkHasItToBeRead = false;
                            fileSize = FileIO::getFileSizeInBytes (
                                customerFTPBinaryPathName, inCaseOfLinkHasItToBeRead);
                        }
                    }
                    catch(exception e)
                    {
                        string errorMessage = string("Error to retrieve the file size")
                            + ", sourceFileName: " + sourceFileName
                        ;
                        _logger->error(__FILEREF__ + errorMessage);
    //
    //                    sendError(500, errorMessage);
    //
    //                    throw runtime_error(errorMessage);            
                    }

                    if (stol(xCatraMMSResumeHeader) == fileSize)
                    {
                        _logger->info(__FILEREF__ + "Resume is enabled"
                            + ", xCatraMMSResumeHeader: " + xCatraMMSResumeHeader
                            + ", fileSize: " + to_string(fileSize)
                        );
                        resume = true;
                    }
                    else
                    {
                        _logger->info(__FILEREF__ + "Resume is NOT enabled (X-CatraMMS-Resume header found but different length)"
                            + ", xCatraMMSResumeHeader: " + xCatraMMSResumeHeader
                            + ", fileSize: " + to_string(fileSize)
                        );
                    }
                }
                else
                {
                    _logger->info(__FILEREF__ + "Resume is NOT enabled (No X-CatraMMS-Resume header found)"
                    );
                }
            }
            
            ofstream binaryFileStream(customerFTPBinaryPathName, 
                    resume ? (ofstream::binary | ofstream::app) : ofstream::binary);
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

                    if (totalRead >= _maxBinaryContentLength)
                    {
                        string errorMessage = string("Binary too long")
                            + ", totalRead: " + to_string(totalRead)
                            + ", _maxBinaryContentLength: " + to_string(_maxBinaryContentLength)
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        sendError(400, errorMessage);

                        throw runtime_error(errorMessage);            
                    }
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

            // rename to .completed
            {
                string customerFTPBinaryPathNameCompleted =
                    customerFTPBinaryPathName;
                customerFTPBinaryPathNameCompleted.append(".completed");
                FileIO::moveFile (customerFTPBinaryPathName, customerFTPBinaryPathNameCompleted);
            }

            unsigned long elapsedUploadInSeconds = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - uploadStartTime).count();
            _logger->info(__FILEREF__ + "Binary read"
                + ", contentLength: " + to_string(contentLength)
                + ", totalRead: " + to_string(totalRead)
                + ", elapsedUploadInSeconds: " + to_string(elapsedUploadInSeconds)
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
                + "\"sourceFileName\": \"" + sourceFileName + "\", "
                + "\"contentLength\": " + to_string(contentLength) + ", "
                + "\"writtenBytes\": " + to_string(totalRead) + ", "
                + "\"elapsedUploadInSeconds\": " + to_string(elapsedUploadInSeconds) + " "
                + "}";
            sendSuccess(201, responseBody);
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
