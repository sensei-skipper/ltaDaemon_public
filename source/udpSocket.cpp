#include "udpSocket.h"

#include "loguru.hpp" //logging

//for inbound connections (LTA simulator)
udpSocket::udpSocket(string ip, string name, unsigned int port)
    : baseSocket(ip, name)
{
    this->port_ = port;
    this->connected_ = false;
}

//for outbound connections (daemon)
udpSocket::udpSocket(string ip, string name, string boardIp, unsigned int boardPort)
    : baseSocket(ip, name)
{
    //this->port_ = port;
    this->port_ = 0; //default to port 0 (let the OS assign a port automatically)

    // Populate sockaddr_in structure.
    memset(&this->boardAddr_, '0', sizeof(this->boardAddr_));
    this->boardAddr_.sin_family = AF_INET;
    this->boardAddr_.sin_port = htons(boardPort); // this is the port that we send to on the LTA - the hardware listens to all ports (so this value doesn't matter), but the ltaSimulator only listens on one port (so we have to set this to something specific)

    //the LTA always sends from port 2001
    if (boardPort==22222) { //special warning for anyone following the old instructions
        LOG_F(WARNING, "You don't need to specify a port - are you sure??\nThe default (2001) is good, and the daemon will not work if you specify a port that does not match the LTA.\nYou should probably just use the LTA IP with -b!");
    }

    if ( inet_aton(boardIp.c_str(), &this->boardAddr_.sin_addr) == 0 )
    {
        LOG_F(ERROR, "Invalid IP/Port combination for board!!");
    }
    this->connected_ = true;
}

int udpSocket::create(unsigned int sec, unsigned int usec)
{
    socklen_t len = sizeof(sockAddr);

    // UDP socket creation.
    sockFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFd < 0)
    {
        LOG_F(ERROR,"Could not create socket");
        return 1;
    }

    // Initialize sockAddr structure.
    // Bind to port 0 to assign it automatically.
    memset(&sockAddr, '0', sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(ip_.c_str());
    //sockAddr.sin_port = 0;
    sockAddr.sin_port = htons(this->port_);

    // Socket timeout.
    struct timeval timeout;
    timeout.tv_sec = sec;
    timeout.tv_usec = usec;
    setsockopt (sockFd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt (sockFd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    if (::bind(sockFd, (struct sockaddr*) &sockAddr, sizeof(sockAddr)) != 0)
    {
        LOG_F(ERROR,"Could not bind socket");
        return -1;
    }

    // Get bound socket information.
    if (getsockname(sockFd, (struct sockaddr*) &sockAddr, &len) != 0)
    {
        LOG_F(ERROR,"Could not get socket name");
        return -1;
    }
    else
    {
        // Copy ip and port to local variables.
        this->ip_ = inet_ntoa(sockAddr.sin_addr);	
        this->port_ = htons(sockAddr.sin_port);
        LOG_F(INFO,"Created UDP socket: %s:%d", this->ip_.c_str(), this->port_);
    }

    if (connected_) {
        if (connect(sockFd, (struct sockaddr *)&boardAddr_, sizeof(boardAddr_)) < 0){ //connect the socket, so we have the local IP address
            LOG_F(INFO,"failed to connect UDP socket");
        } else {
            // Get bound socket information.
            if (getsockname(sockFd, (struct sockaddr*) &sockAddr, &len) != 0)
            {
                LOG_F(ERROR,"Could not get socket name");
            }
            else
            {
                // Copy ip and port to local variables.
                this->ip_ = inet_ntoa(sockAddr.sin_addr);	
                this->port_ = htons(sockAddr.sin_port);
                LOG_F(INFO,"Updated UDP socket: %s:%d", this->ip_.c_str(), this->port_);
            }

        }
    }
    return 0;
}

int udpSocket::flush()
{
    char dataBuff[LTA_SOFT_DATABUFF_SIZE];
    sockaddr_in clientAddr;
    unsigned int clientAddrLen = sizeof(clientAddr);
    // clear stale data from the socket
    int staleBytes = 0;
    while (true) {
        int numRecBytes = recvfrom(sockFd, &dataBuff, LTA_SOFT_DATABUFF_SIZE, 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (numRecBytes<0) {
            break;
        } else {
            staleBytes += numRecBytes;
        }
    }
    LOG_F(INFO, "discarded %d stale bytes from %s socket", staleBytes, name_.c_str());
    return 0;
}
