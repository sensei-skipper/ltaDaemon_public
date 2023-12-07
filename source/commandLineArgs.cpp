#include "commandLineArgs.h"
#include <cstring>

#include "loguru.hpp" //logging

int parseAddr(const char* addr, string &IP, unsigned int &port)
{
    //we expect an integer port, a dotted-quad IP, or IP:port

    //if we were just given a port, parsePort will return 0
    if (parsePort(addr, port)==0) {
        return 0;
    }

    //if the string contains a :, split it
    const char *split = strchr(addr, ':');
    if (split != NULL) {
        IP = std::string(addr, split-addr);
        return parsePort(split+1, port);
    }

    //if we get here, we must have been given just the IP
    IP = addr;
    return 0;
}

int parsePort(const char* addr, unsigned int &port) {
    char *p;
    long value = strtol(addr, &p, 10);
    if (*p=='\0') {//string was a valid integer
        port = value;
        return 0;
    } else {
        return 1;
    }
}

