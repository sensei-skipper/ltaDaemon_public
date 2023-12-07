#include "tcpSocket.h"

#include "loguru.hpp" //logging

tcpSocket::tcpSocket(string ip, string name, unsigned int port)
    : baseSocket(ip, name)
{
    this->port_ = port;
    this->ip_ = ip;
}

int tcpSocket::create()
{
    socklen_t len = sizeof(sockAddr);

    // TCP socket creation.
    sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0)
    {
        LOG_F(ERROR,"Could not create socket");
        return 1;
    }

    // Initialize sockAddr structure.
    memset(&sockAddr, '0', sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    //sockAddr.sin_addr.s_addr = INADDR_ANY; //listen on all IP addresses
    if ( inet_aton(this->ip_.c_str(), &sockAddr.sin_addr) == 0 ) //bind to the specific IP address
    {
        LOG_F(ERROR,"Invalid IP address");
        return 1;
    }
    sockAddr.sin_port = htons(this->port_);

    // Socket timeout.
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;

    // Set socket options.
    int opt = 1;
    setsockopt (sockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt (sockFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt (sockFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // Bind socket to address and port.
    if (::bind(sockFd, (struct sockaddr*) &sockAddr, sizeof(sockAddr)) != 0)
    {
        LOG_F(ERROR,"Could not bind socket");
        return -1;
    }

    // Start listening on socket.
    listen(sockFd, 10);

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
        LOG_F(INFO,"Created TCP socket: %s:%d", this->ip_.c_str(), this->port_);
    }

    return 0;
}

