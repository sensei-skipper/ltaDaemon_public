#ifndef _dumpToDisc_h_
#define _dumpToDisc_h_

#include <string>
#include <vector>
#include "udpSocket.h"
#include <csignal>

#define LTA_BIN_FILE_SUFFIX ".dat"
#define LTASOFT_DELAY_BEFORE_USER_CANCELLING_READOUT 1000000 //in us
#define LTA_SOFT_MAXIMUM_BINARY_FILE_SIZE 50000000 //maximum size of files in bytes

typedef struct
{
    udpSocket * dataSocket;
    const string * outFileName;
    vector<string> * inFileNames;
    const long * sampsExpected;
    volatile std::sig_atomic_t * readingFlag;
    volatile std::sig_atomic_t * readoutStatus;
}  readoutArgs_t;

class DumpToDisc
{
    public:
        DumpToDisc(udpSocket * dataSocket):dataSocket_(dataSocket),readingFlag(0),readoutStatus(0){};
        bool is_readout_active();
        bool is_readout_getting_data();
        bool is_readout_done();
        int start_readout_thread(const int &client_sock, const string outFileName, vector<string> * inFileNames, const long &sampsExpected);
        int stop_readout_thread();

    private:
        udpSocket * dataSocket_;
        pthread_t readoutThread;
        readoutArgs_t readoutArgs;
        volatile std::sig_atomic_t readingFlag; //main thread sets this to 0 to tell readout thread to exit

        volatile std::sig_atomic_t readoutStatus; //readout thread sets this, main thread reads it
        // 0: initial value
        // 1: ready for data
        // 2: getting data
        // 3: readout done (timed out)
};

int closeFile(std::ofstream * outFile);
std::ofstream * createBinFile(const string &fileName, const int &numbering, vector<string> *inFileNames);
void *saveData(void * args);

#endif
