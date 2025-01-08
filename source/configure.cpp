#include <unistd.h>
#include <string.h>

#include "loguru.hpp" //logging

#include "userInterpreter.h"
#include "ethernet.h"
#include "commandLineArgs.h"
#include "udpSocket.h"
#include "tcpSocket.h"
#include "ltaBoard.h"

using namespace std;

commandLineArgs::commandLineArgs()
{
    daqIP_        = CMD_LINE_DAQ_IP_DEFAULT;

    parseAddr(CMD_LINE_BOARD_ADDR_DEFAULT, this->boardIP_, this->boardPort_);
    parseAddr(CMD_LINE_TERMINAL_ADDR_DEFAULT, this->terminalIP_, this->terminalPort_);
}

int commandLineArgs::processArgs(const int argc, char *argv[])
{
    int opt = 0;

    while ( (opt = getopt(argc, argv, "hb:l:t:")) != -1 )
    {
        int errStatus = 0;
        switch (opt)
        {
            case 'h':
                cout << "configure.exe <command>" << endl;
                cout << "\t-b <ip:port>: Board address (LTA address for control, must be reachable from DAQ PC, default " << CMD_LINE_BOARD_ADDR_DEFAULT << ")" << endl;
                cout << "\t-l <ip>: DAQ IP (PC address for data, must be reachable from LTA, default " << CMD_LINE_DAQ_IP_DEFAULT << ")" << endl;
                cout << "\t-t <ip:port>: Terminal address (PC address for terminal, must be reachable from client PC, default " << CMD_LINE_TERMINAL_ADDR_DEFAULT << ")" << endl;
                cout << "\t-b and -t options will accept IP only or port only, you don't need to specify both" << endl;
                return 2;
            case 'b':
                errStatus = parseAddr(optarg, this->boardIP_, this->boardPort_);
                if( errStatus!=0 ){
                    LOG_F(ERROR, "Problem parsing board address");
                    return 1;
                } 
                break;

            case 'l':
                this->daqIP_ = optarg;
                break;

            case 't':
                errStatus = parseAddr(optarg, this->terminalIP_, this->terminalPort_);
                if( errStatus!=0 ){
                    LOG_F(ERROR, "Problem parsing teminal address");	
                    return 1;
                }
                break;	
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    loguru::init(argc, argv, "-V");
    loguru::set_thread_name("configure");
    char log_path[PATH_MAX];
    loguru::suggest_log_path("logs", log_path, sizeof(log_path));
    loguru::add_file(log_path, loguru::Append, loguru::Verbosity_MAX);


    // Get arguments from command line.
    commandLineArgs cmdLine;
    int returnCode = cmdLine.processArgs( argc, argv);
    if (returnCode == 1)
    {
        LOG_F(ERROR, "Error processing command line arguments!!");
    }
    if (returnCode != 0) {
        return 1;
    }

    LOG_F(INFO, "Board IP: %s", cmdLine.boardIP().c_str());
    LOG_F(INFO, "Board Port: %d", cmdLine.boardPort());
    LOG_F(INFO, "DAQ IP: %s", cmdLine.daqIP().c_str());
    LOG_F(INFO, "Terminal IP: %s", cmdLine.terminalIP().c_str());
    LOG_F(INFO, "Terminal Port: %d", cmdLine.terminalPort());

    // Socket to send/receive control messages to/from board.
    udpSocket* boardCtrl = new udpSocket(cmdLine.daqIP(), "LTA control", cmdLine.boardIP(), cmdLine.boardPort());
    //requirements for data socket timeout:
    //must be at least 100 ms, since the last data packet seems to arrive 100 ms after the end of readout
    //you want this as long as possible to allow for gaps in LTA data transmission (e.g. between rows)
    //should be less than 1 sec, since otherwise the LTA handshake reports a problem (though the message is not actually lost)
    //must be less than the LTA's output buffer timeout (10 seconds), otherwise we will lose the "Read done" message after end of readout
    //requirements for control socket timeout:
    //doesn't really matter? normally the LTA responds quite quickly when polled
    boardCtrl->create(0, 800000);
    boardCtrl->flush();

    //boardCtrl->printSockName();
    // Socket to receive data from board.
    udpSocket* boardData = new udpSocket(cmdLine.daqIP(), "LTA data", cmdLine.boardIP(), cmdLine.boardPort());
    boardData->create(0, 800000);

    // Socket to receive commands from terminal.
    tcpSocket* softCtrl = new tcpSocket(cmdLine.terminalIP(), "terminal", cmdLine.terminalPort());
    softCtrl->create();

    // Initialize board object.
    ltaBoard* boardPtr = new ltaBoard (	cmdLine.boardIP(), 
            cmdLine.boardPort(),
            boardCtrl,
            boardData);

    Ethernet * eth = new Ethernet(boardCtrl->fd());

    // Configure board.
    if ( eth->configureBoard(boardPtr) != 0 )
    {
        LOG_F(ERROR, "Could not configure board!!");
        return 1;
    }

    //boardCtrl->printSockName();
    UserInterpreter * userInterpreter = new UserInterpreter(softCtrl->fd(), eth, boardData);

    // Main Loop.
    string responseBuff;
    while (true) {
        if (userInterpreter->listenForCommands() == -1) {
            LOG_F(INFO, "closing lta software ");
            break;
        }

        responseBuff.clear();
        while (eth->recvCmdEthe(responseBuff) == 0) //pull text from the LTA until it has no more
        {
            //trim trailing newlines
            size_t pos = responseBuff.find_last_not_of("\r\n");
            if (pos!=string::npos)
                responseBuff.erase(responseBuff.find_last_not_of("\r\n")+1,string::npos);
            LOG_F(INFO, "%s", responseBuff.c_str());
            responseBuff.clear();
            usleep(10000);
        }
    }

    //clean up
    delete userInterpreter;
    delete eth;
    delete boardPtr;
    delete softCtrl;
    delete boardData;
    delete boardCtrl;


    return 0;
}
