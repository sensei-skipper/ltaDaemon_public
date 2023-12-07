#include <fstream>
#include <sstream>
#include <unistd.h> //close (on OS X)
#include <vector>
#include <cstdio>
#include <string.h>
#include <iostream>
#include <stdlib.h>   /*strtol*/
#include <cmath>

#include<sys/socket.h>  //socket
#include<arpa/inet.h>   //inet_addr

#include <pthread.h>

#include <limits.h> //PATH_MAX

#include "loguru.hpp" //logging

#include "dumpToDisc.h"
#include "helperFunctions.h"

#define READOUT_TIMEOUT 10.0 //maximum time between data packets, in seconds

using namespace std;


int closeFile(std::ofstream * outFile)
{
    if(outFile->is_open())
    {
        outFile->close();
    }
    delete outFile;
    return 0;
}

std::ofstream * createBinFile(const string &fileName, const int &numbering, vector<string> *inFileNames)
{
    std::ofstream * outFile = new ofstream;
    char newFileName[PATH_MAX];
    sprintf(newFileName, "%s_%d%s", fileName.c_str(), numbering, LTA_BIN_FILE_SUFFIX);
    if (fileExist(newFileName)) {
        LOG_F(WARNING, "Will overwrite: %s", newFileName);
        deleteFile(newFileName);
    }
    outFile->open(newFileName, std::ios::out | std::ios::binary);
    if (!outFile->is_open())
    {
        LOG_F(ERROR, "cannot open file to save data, exiting...");
        delete outFile;
        return NULL;
    }
    //save file names for postProcessing
    inFileNames->push_back(newFileName);
    LOG_F(INFO, "file created %s", newFileName);
    return outFile;
}

int DumpToDisc::start_readout_thread(const int &client_sock, const string outFileName, vector<string> * inFileNames, const long &sampsExpected) {
    if (is_readout_active()) {
        LOG_F(ERROR, "currently reading or transfering data (readingFlag=%d), failed to start readout", readingFlag);
        string response("currently reading or transfering data...nothing done\n");
        write(client_sock , response.c_str() , response.size());
        return readingFlag;
    }
    // Start readout.

    readoutArgs.dataSocket = dataSocket_;
    readoutArgs.outFileName = &outFileName;
    readoutArgs.inFileNames = inFileNames;
    readoutArgs.sampsExpected = &sampsExpected;
    readoutArgs.readingFlag = &readingFlag;
    readoutArgs.readoutStatus = &readoutStatus;

    readingFlag = 1;
    readoutStatus = 0;

    //clear list of binary files
    inFileNames->clear();

    // Create readout thread.
    int status = pthread_create( &readoutThread, NULL, saveData, (void*) &readoutArgs);

    // Wait until the routine is ready to get packages.
    LOG_F(INFO,"waiting for readout thread to start");
    while (readoutStatus < 1) { //0=initializing, -1=failed
        if (readoutStatus == -1)
        {
            string response;
            response.clear();
            response = "something went wrong initializing the data file \nno operation on the detector";

            // Send response to terminal.
            write(client_sock , response.c_str() , response.size());
            write(client_sock ,  "\n", 1);

            close(client_sock);
            readingFlag = 0;
            pthread_join(readoutThread, NULL);
            return 1;
        }
        usleep(1000);
    }
    return status;
}

bool DumpToDisc::is_readout_active() {
    return (readingFlag!=0 || readoutStatus!=0);
}

bool DumpToDisc::is_readout_getting_data() {
    return (readoutStatus>=2);
}

bool DumpToDisc::is_readout_done() {
    //LOG_F(INFO, "readoutStatus %d", readoutStatus);
    return (readoutStatus==3);
}

int DumpToDisc::stop_readout_thread() {
    if (is_readout_active()) {
        LOG_F(INFO, "requesting readout thread to exit");
        readingFlag = 0;
        pthread_join(readoutThread, NULL);
        LOG_F(INFO, "readout thread exited");
    }
    return 0;
}

void *saveData(void * args)
{
    readoutArgs_t * readoutArgs = (readoutArgs_t *) args;

    udpSocket * dataSocket = readoutArgs->dataSocket;
    const string outFileName = *(readoutArgs->outFileName);
    vector<string> * inFileNames = readoutArgs->inFileNames;
    const long sampsExpected = *(readoutArgs->sampsExpected);
    volatile std::sig_atomic_t * readingFlag = readoutArgs->readingFlag;
    volatile std::sig_atomic_t * readoutStatus = readoutArgs->readoutStatus;

    loguru::set_thread_name("readout thread");

    int currentSubRunNumber = 0;
    std::ofstream * outFile = createBinFile(outFileName, currentSubRunNumber, inFileNames);
    if (outFile==NULL)
    {
        *readoutStatus = -1;
        LOG_F(ERROR, "failed to create output file");
        pthread_exit(NULL);
    }

    //start receiving data from ethernet
    char dataBuff[LTA_SOFT_DATABUFF_SIZE];
    sockaddr_in clientAddr;
    unsigned int clientAddrLen = sizeof(clientAddr);

    // clear stale data from the socket
    dataSocket->flush();

    *readoutStatus = 1;
    LOG_F(INFO, "readout thread is ready for data");

    int packCount = 0;
    unsigned long bytesReadThisFile = 0;
    unsigned long sampsRead = 0;
    timespec timeoutTime; //
    while (*readingFlag == 1) {

        if(LTA_SOFT_MAXIMUM_BINARY_FILE_SIZE > 0)
        {
            //long length = outFile.tellp();
            if(bytesReadThisFile >= LTA_SOFT_MAXIMUM_BINARY_FILE_SIZE)
            {
                closeFile(outFile);
                ++currentSubRunNumber;
                outFile = createBinFile(outFileName, currentSubRunNumber, inFileNames);
                bytesReadThisFile = 0;
            }
        }

        int numRecBytes = recvfrom(dataSocket->fd(), &dataBuff, LTA_SOFT_DATABUFF_SIZE, 0, (sockaddr*)&clientAddr, &clientAddrLen);
        if (numRecBytes > 0) {
            switch (*readoutStatus) {
                case 0: //shouldn't get here
                case 2:
                    break;
                case 1:
                    LOG_F(INFO, "first data packet received");
                    break;
                case 3:
                    LOG_F(WARNING, "got a data packet after we thought the readout had finished!");
                    break;
            }
            *readoutStatus = 2;
            get_end_time(&timeoutTime, READOUT_TIMEOUT); //we got some data, so update the timeout
            //write the number of quadwords in the udp package before saving the data
            if ((numRecBytes % 8) != 2) {
                LOG_F(WARNING, "unexpected packet size %d (should normally be 2 mod 8)", numRecBytes);
            }
            unsigned char quadWordsCount = (numRecBytes - 2) / 8;
            //LOG_F(1, "numRecBytes=%d, packType=%d, packCounter=%d", numRecBytes, dataBuff[0], (unsigned char) dataBuff[1]);
            outFile->write( (char*)&quadWordsCount, 1);
            outFile->write(dataBuff, numRecBytes);
            packCount ++;
            bytesReadThisFile += numRecBytes;
            sampsRead += quadWordsCount;
        } else if (numRecBytes == -1) {
            if (errno==EAGAIN || errno==EWOULDBLOCK) {//UDP timeout
                switch (*readoutStatus) {
                    case 0: //shouldn't get here
                    case 1:
                    case 3:
                        break;
                    case 2:
                        //if we don't have the correct amount of data (either too much or too little), wait for the full duration of READOUT_TIMEOUT - we want to see if we get more data
                        //if we have the correct amount of data, we are happy to just wait for the duration of one UDP timeout
                        if (time_is_up(&timeoutTime) || sampsRead==sampsExpected) {
                            LOG_F(INFO, "no data in a while: readout done? we got %ld samples, expected %ld", sampsRead, sampsExpected);
                            *readoutStatus = 3;
                        }
                        break;
                }
            } else {
                LOG_F(WARNING, "UDP error on data port, no message received: %s", strerror(errno));
            }
        } else {
            LOG_F(WARNING, "unexpected numRecBytes=%d", numRecBytes);
        }
    }

    closeFile(outFile);
    LOG_F(INFO, "all data collected (got %ld samples), readout thread exiting", sampsRead);

    //return 0;
    *readoutStatus = 0;
    pthread_exit(NULL);
}
