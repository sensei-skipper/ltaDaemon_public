#include <unistd.h>
#include <string.h>
#include <iostream>

#include "helperFunctions.h"
#include "smartSequencerHandler.h"
#include "loguru.hpp" //logging

#include <chrono>
#include <thread>

using namespace std;


void printHelp(const char *exeName){

    cout << endl;
    cout << "==========================================================================\n"
         << "This program reads an XML smart seqencer file and computes how long it\n"
         << "willtake to execute when run by the LTA.\n";
    cout << "==========================================================================\n\n"
         << "Usage:\n"
         << "  "   << exeName << " <input XML file>\n\n"
         << "Options:\n"
         << "  -v <verbosity> for displaying detailed information.\n"
         << "                 <verbosity> is an integer between 1 and 3.\n\n"
         << "==========================================================================\n\n";
}


int processCommandLineArgs(int argc, char *argv[]){

        if(argc == 1) return 1;

        int verbosity=0;
        int opt=0;
        while ( (opt = getopt(argc, argv, "v:")) != -1) {
            switch (opt) {
                case 'v':
                    verbosity = atoi(optarg);
                    break;
                default: /* '?' */
                    cerr << "For help use -h option.\n\n";
                    return 3;
            }
        }

        if(argc-optind==0){
            cerr << "Error: no input seqencer file provided!\n\n";
            return 1;
        }else if(argc-optind>1){
            cerr << "Error: more than one input file provided!\n\n";
            return 1;
        }

        string inFile=argv[optind];
        if(!fileExist(inFile.c_str())){
            cout << "\nError reading input seqencer file: " << inFile <<"\nThe file doesn't exist!\n\n";
            return -1;
        }

        if(verbosity!=0){
            loguru::init(argc, argv);
            loguru::set_thread_name("seqTools");
        }

        return 0;
    }


int main(int argc, char* argv[])
{
    int returnCode = processCommandLineArgs( argc, argv);
    if(returnCode!=0){
        if(returnCode>0) printHelp(argv[0]);
        return returnCode;
    }

    smartSequencerHandler seqHand;

    const string seqFileName(argv[1]);
    seqHand.loadNewSequencer(seqFileName);

    cout << "\n======================================================================\n";
    cout << "  It will take " << seqHand.calculateSequencerDuration() << "s (" <<printTime(seqHand.calculateSequencerDuration()) << ") to execute this seqencer\n";
    cout << "  It will take " << seqHand.calculateSequencerTimeUntilFirstData() << "s (" <<printTime(seqHand.calculateSequencerTimeUntilFirstData()) << ") to get the first data package\n";
    cout << "======================================================================\n";
    return 0;
}
