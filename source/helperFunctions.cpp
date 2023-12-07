#include <stdio.h>
#include <vector>
#include <string.h>
#include <string>
#include <cmath>
#include <unistd.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

#include <sys/stat.h> //struct stat

#include <time.h>

#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "loguru.hpp" //logging

#include "helperFunctions.h"

bool fileExist(const char *fileName) {
    struct stat buffer;   
    return (stat(fileName, &buffer) == 0);
}

int deleteFile(const char *fileName) {
    return unlink(fileName);
}

void trimString(std::string &str) {
    const std::string whitespace = " \n\r\t";
    //trim whitespace
    str.erase(str.find_last_not_of(whitespace)+1);
    str.erase(0,str.find_first_not_of(whitespace));
}

std::vector<std::string> tokenize(char * input) {
    //separar recvBuff en palabras
    std::vector<std::string> words;
    char *token;
    char *rest;
    rest = input;
    while( (token = strtok(rest, " \r\n")) ) {
        rest = NULL;
        std::string word = token;
        trimString(word);
        words.push_back(word);
    }
    return words;
}

//returns monotonically increasing time, for timers
static void get_current_time(struct timespec *ts) {
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_MONOTONIC, ts);
#endif
}

void get_end_time(struct timespec *ts, const double deltaT) {
    const long ns_per_sec = 1000000000L;
    get_current_time(ts);
    int sec = (int) deltaT;
    long nsec = ts->tv_nsec + ((deltaT-sec) * ns_per_sec);
    if (nsec > ns_per_sec) {
        ts->tv_nsec = nsec - ns_per_sec;
        ts->tv_sec = ts->tv_sec + sec + 1;
    } else {
        ts->tv_nsec = nsec;
        ts->tv_sec = ts->tv_sec + sec;
    }
}

bool time_is_up(struct timespec *end_time) {
    timespec current_time;
    get_current_time(&current_time);
    //LOG_F(1, "now: %ld secs, %ld nsecs end: %ld secs, %ld nsecs", current_time.tv_sec, current_time.tv_nsec, end_time->tv_sec, end_time->tv_nsec);
    return (current_time.tv_sec>end_time->tv_sec || (current_time.tv_sec==end_time->tv_sec && current_time.tv_nsec>end_time->tv_nsec));
}

std::string lowercase(const std::string value)
{
    //Convert entire string to lowercase
    std::string lower(value);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return lower;
}

std::string uppercase(const std::string value)
{
    //Convert entire string to uppercase
    std::string upper(value);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return upper;
}

std::string bool_to_string(const bool value) {
    // Convert boolean value to string ("true", "false").
    return value ? "true" : "false";
}

bool string_to_bool(const std::string value) {
    // Convert string to boolean value.
    // Input can be (case insensitive) "true", "false", "0", "1"
    std::string lower = lowercase(value);
    
    if (lower.compare("true") == 0) {
        return true;
    } else if (lower.compare("false") == 0) {
        return false;
    } else if (std::atoi(lower.c_str()) > 0) {
        return true; 
    } else {
        return false;
    }
}

std::string printTime(float seconds) {
    const float us = 1e-6;
    const float ms = 1e-3;
    const float s  = 1;
    const float minute = 60;
    const float hour = 3600;
    const float day = 86400;
    std::stringstream ss;
    if (seconds < ms) {
        ss << std::fixed << std::setprecision(2) << seconds/us << "us";
    } else if (seconds < s) {
        ss << std::fixed << std::setprecision(2) << seconds/ms << "ms";
    } else if (seconds < minute) {
        ss << std::fixed << std::setprecision(2) << seconds/s << "s";
    } else if (seconds < hour) {
        ss << std::fixed << std::setprecision(2) << seconds/minute << "min";
    } else if (seconds < day) {
        ss << std::fixed << std::setprecision(2) << seconds/hour << "h";
    } else {
        ss << std::fixed << std::setprecision(2) << seconds/day << "d";
    } 

    return ss.str();
}