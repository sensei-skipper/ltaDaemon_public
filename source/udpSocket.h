#ifndef _udpSocket_h_
#define _udpSocket_h_

#include <string>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "baseSocket.h"

#define LTA_SOFT_DATABUFF_SIZE 2000

using namespace std;

class udpSocket : public baseSocket
{
    public:
        udpSocket(string ip, string name, unsigned int port);
        udpSocket(string ip, string name, string boardIp, unsigned int boardPort);

        int create(unsigned int sec, unsigned int usec);
        int create() {return create(0,0);};

        int flush();
        sockaddr_in remoteaddr() {return boardAddr_;}

    private:
        bool connected_; //true if the socket is connected to the LTA
        sockaddr_in boardAddr_;
};

#endif

