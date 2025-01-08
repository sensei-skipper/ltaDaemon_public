#ifndef _helperFunctions_h_
#define _helperFunctions_h_

#include <vector>
#include <string.h>

bool fileExist(const char *fileName);

int deleteFile(const char *fileName);

void trimString(std::string &str, const std::string &tail="");

std::vector<std::string> tokenize(char * input);

//fills ts with the time deltaT seconds from now
void get_end_time(struct timespec *ts, const double deltaT);

//true if the current time is after ts
bool time_is_up(struct timespec *end_time);

//change case of string
std::string lowercase(const std::string value);
std::string uppercase(const std::string value);

//covert bool to string and back
std::string bool_to_string(const bool value);
bool string_to_bool(const std::string value);

//picks the time units so it's easier to read
std::string printTime(float seconds);

//replaces all the special characters with their escaped version
std::string escapeSpecialCharacters(const std::string& input);

#endif
