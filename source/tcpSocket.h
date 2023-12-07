#ifndef _tcpSocket_h_
#define _tcpSocket_h_

#include <string>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "baseSocket.h"

using namespace std;

class tcpSocket : public baseSocket
{
public:
	tcpSocket(string ip, string name, unsigned int port);

	int create();
};

#endif

