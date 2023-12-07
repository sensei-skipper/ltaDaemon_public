#ifndef _masterLtaCmdLine_h_
#define _masterLtaCmdLine_h_

#include <iostream>

#define CMD_LINE_BOARD_CODE_DEFAULT 	"all"
#define CMD_LINE_CONFIG_FILE_DEFAULT 	"ltaConfigFile.txt"

using namespace std;

class masterLtaCmdLine
{
    public:
        masterLtaCmdLine(): boardCode_(CMD_LINE_BOARD_CODE_DEFAULT), configFile_(CMD_LINE_CONFIG_FILE_DEFAULT){};

        int processArgs(const int argc, char *argv[]);

        // Getters.
        string boardCode(){return boardCode_;};
        string boardCmd(){return boardCmd_;};
        string configFile(){return configFile_;};

    private:
        // Arguments.
        string boardCode_;
        string boardCmd_;
        string configFile_;
};

#endif

