#include "masterLtaCmdLine.h"
#include <vector>
#include <sstream>
#include <iostream>
#include <unistd.h>

#include "loguru.hpp" //logging

int masterLtaCmdLine::processArgs(const int argc, char *argv[])
{
    int opt = 0;

    // Parse options.
    while ( (opt = getopt(argc, argv, "hb:c:")) != -1 )
    {
        switch (opt)
        {
            case 'h':
                cout << "masterLta.exe <command>" << endl;
                cout << "\t-b <boardcode>: send to the board with this code, \"all\" sends to all boards (default " << CMD_LINE_BOARD_CODE_DEFAULT << ")" << endl;
                cout << "\t-c <conffile>: use specified config file (default " << CMD_LINE_CONFIG_FILE_DEFAULT <<")" << endl;
                return 1;
            case 'b':
                boardCode_ = optarg;			
                break;
            case 'c':
                configFile_ = optarg;			
                break;
        }
    }

    // The rest of the string is considered a command.
    if ( (argc-optind) > 0 )
    {
        boardCmd_ = "";
        for (int i=optind; i<argc; i++)
        {
            boardCmd_ = boardCmd_ + argv[i] + " ";
        }
    }
    else
    {
        LOG_F(ERROR, "At least one command must be provided.");
        return -1;
    }

    return 0;
}
