#ifndef _baseSocket_h_
#define _baseSocket_h_

#include <string>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>

using namespace std;

class baseSocket
{
public:
	baseSocket(string ip, string name): ip_(ip), name_(name){};

	virtual int create() = 0;

	string ip() {return ip_;};
	unsigned int port() {return port_;};
	int fd() {return sockFd;};
	sockaddr_in& sockaddr() {return sockAddr;};
        virtual ~baseSocket(){};

protected:
	string ip_;
        string name_;
	unsigned int port_;
	int sockFd;
	struct sockaddr_in sockAddr;
};

#endif

