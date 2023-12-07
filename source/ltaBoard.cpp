#include "ltaBoard.h"
#include <string.h>

#include "loguru.hpp" //logging

ltaBoard::ltaBoard(string ip, unsigned int port, udpSocket* ctrlSocket, udpSocket* dataSocket)
{
    this->ip_ = ip;
    this->port_ = port;
    this->ctrlSocket_ = ctrlSocket;
    this->dataSocket_ = dataSocket;
}

string ltaBoard::ip()
{
    return ip_;
}

string ltaBoard::ctrlIp()
{
    return ctrlSocket_->ip();
}

unsigned int ltaBoard::ctrlPort()
{
    return ctrlSocket_->port();
}

string ltaBoard::dataIp()
{
    return dataSocket_->ip();
}

unsigned int ltaBoard::dataPort()
{
    return dataSocket_->port();
}
