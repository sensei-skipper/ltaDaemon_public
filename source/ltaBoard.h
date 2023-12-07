#ifndef _ltaBoard_h_
#define _ltaBoard_h_

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "udpSocket.h"

using namespace std;

class ltaBoard
{
public:
	ltaBoard(string ip, unsigned int port, udpSocket* ctrlSocket, udpSocket* dataSocket);

	string ip();
	unsigned int port();
	string ctrlIp();
	unsigned int ctrlPort();
	string dataIp();
	unsigned int dataPort();

private:
	string ip_;
	unsigned int port_;
    udpSocket* ctrlSocket_;
    udpSocket* dataSocket_;
};

#endif

