#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <ctime>
#include <vector>
#include <sys/file.h>
#include <streambuf>

#include "loguru.hpp" //logging

#include "masterLtaCmdLine.h"
#include "masterLtaClass.h"
#include "helperFunctions.h"

using namespace std;

/*! Try to get lock. Return its file descriptor or -1 if failed.
 *
 *  @param lockName Name of file used as lock (i.e. '/var/lock/myLock').
 *  @return File descriptor of lock file, or -1 if failed.
 */
int tryGetLock( char const *lockName )
{
    mode_t m = umask( 0 );
    int fd = open( lockName, O_RDWR|O_CREAT, 0666 );
    umask( m );
    if( fd >= 0 && flock( fd, LOCK_EX | LOCK_NB ) < 0 ){
        close( fd );
        fd = -1;
    }
    return fd;
}

int main(int argc, char* argv[])
{
    loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;
    loguru::init(argc, argv);
    loguru::add_file("masterLta.log", loguru::Append, loguru::Verbosity_MAX);
    loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
    loguru::g_internal_verbosity = 1;

    // We want to allow only one instance of the master lta
    // control software to be able to run at the same time
    if( tryGetLock("/tmp/masterLta.lock") == -1){
        LOG_F(ERROR, "Can't get a lock on the lock file. Will not continue.\nCheck if there is another instance of the masterLta running.");
        return 1;
    }

    // Get command line arguments.
    masterLtaCmdLine cmdLine;
    int returnCode = cmdLine.processArgs(argc, argv);
    if ( returnCode != 0)
    {
        if (returnCode<0) LOG_F(ERROR, "Error processing command line arguments.");
        return 1;
    }

    int status = 0;

    // Read configuration file and build system map.
    masterLta_t mlta(cmdLine.configFile());
    //mlta.printStatus();
    if (mlta.getStatus()!=eOK) {
        return 1;
    }

    // Check for broadcast.
    if ( strcmp(cmdLine.boardCode().c_str(), "all") == 0 )
    {
        mlta.broadcast(cmdLine.boardCmd());
        for (int index = 0; index < mlta.getN(); ++index){
            stringstream printstream;
            printstream << mlta.getLta(index).resp; 
            string outstr = printstream.str();
            trimString(outstr);//get rid of leading and trailing whitespace
            LOG_IF_F(INFO, outstr.length()>0, "%s", outstr.c_str());
        }
    }
    else
    {
        int index = mlta.codeToIndex(cmdLine.boardCode());
        if (index < 0) {
            LOG_F(ERROR, "Invalid board code");
            return 1;
        }
        mlta.singleSend(index, cmdLine.boardCmd());
        stringstream printstream;
        printstream << mlta.getLta(index).resp << endl; 
        string outstr = printstream.str();
        trimString(outstr);//get rid of leading and trailing whitespace
        LOG_IF_F(INFO, outstr.length()>0, "%s", outstr.c_str());
    }


    return status;
}
