#ifndef _commandLineArgs_h_
#define _commandLineArgs_h_

#include <iostream>
#include <unistd.h>

/*
	Arguments:
	-b <ip>
	-t <ip:port>
*/

#define CMD_LINE_BOARD_ADDR_DEFAULT		"192.168.133.7:2001"
#define CMD_LINE_DAQ_IP_DEFAULT 		"0.0.0.0"
#define CMD_LINE_TERMINAL_ADDR_DEFAULT 	"127.0.0.1:8888"

using namespace std;

class commandLineArgs
{
public:
	commandLineArgs();

	int processArgs(const int argc, char *argv[]);

	// Getters.
	string boardIP(){return this->boardIP_;};
	unsigned int boardPort(){return this->boardPort_;};
	string daqIP(){return this->daqIP_;};
	string terminalIP(){return this->terminalIP_;};
	unsigned int terminalPort(){return this->terminalPort_;};

private:
	// Arguments.
	string boardIP_;
	unsigned int  boardPort_;
	string daqIP_;
	string terminalIP_;
	unsigned int terminalPort_;
};

int parseAddr(const char* addr, string &IP, unsigned int &port);
int parsePort(const char* addr, unsigned int &port);
#endif

