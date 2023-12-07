#include <fstream>
#include <sstream>
#include <vector>
#include <set>

#include "loguru.hpp" //logging

#include "helperFunctions.h"
#include "commandLineArgs.h"
#include "tcpSocket.h"

#include "masterLtaClass.h"

using namespace std;

ostream & operator << (ostream &out, const ltaReply_t &c) 
{
    if (c.sendStatus_!=0 || c.replyStatus_!=0 || c.replyMsg_.length()>0)
        out << "[" << c.code_ << "]\tSend:" << c.sendStatus_ <<"\tReply:" << c.replyStatus_ << "\tMsg: " << c.replyMsg_; 
    return out; 
} 

ostream & operator << (ostream &out, const ltaConfigEntry_t &c) 
{ 
    out << c.code_ << "\t" << c.ip_ << "\t" << c.port_; 
    return out; 
} 

masterLta_t::masterLta_t(const string &confFile) : status_(eUninitialized)
{
    int readConfStatus = readLtaConfFile(confFile);
    if(readConfStatus==0) status_ = eOK;
    else status_ = eError;

    return;
} 

int masterLta_t::readLtaConfFile(const string &confFile){
    if(!fileExist(confFile.c_str())){
        LOG_F(ERROR, "Error reading input file: %s\nThe file doesn't exist!",confFile.c_str());
        return false;
    }
    ifstream in(confFile.c_str());

    //for sanity checks
    std::set<string> addrMap;

    while(!in.eof()){
        string line;
        getline(in, line);

        //trim off any commented portion of this line
        size_t found = line.find("#");
        if (found != string::npos) line = line.substr(0,found);

        vector<string> words;
        std::stringstream iss(line);
        string aux;
        while (iss >> aux) {
            words.push_back(aux);
        }
        if (words.size()==0) continue; //empty or commented line
        if(words.size() < 3){ //masterLta uses the first 3 columns and ignores the rest (normally the LTA IPs go in the 4th column)
            LOG_F(ERROR, "Invalid row in config file, only %d words", words.size());
            in.close();
            return 1;
        }

        ltaConfigEntry_t confLine(words[0], words[1], atoi(words[2].c_str()));
        int sock;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){ 
            LOG_F(ERROR, "Socket creation error");
            return 1; 
        } 
        ltaReply_t resp;
        lta_t lta = {confLine, sock, resp};
        ltas_.push_back(lta);


        code2index_[confLine.code()] = ltas_.size() - 1;

        //sanity checks
        ostringstream auxOSS;
        auxOSS << confLine.ip() << ":" << confLine.port();
        addrMap.insert(auxOSS.str());
    }

    /* Check config info consistency */
    /* and print config info */

    CHECK_EQ_F(code2index_.size(), ltas_.size(), "Config error: Two boards have the same code.");
    CHECK_EQ_F(addrMap.size(), ltas_.size(), "Config error: Two boards have the same IP+port.");

    return 0;
}

int masterLta_t::broadcast(const string msg){

    int msgStatus = 0;

    for (uint i = 0; i < ltas_.size(); ++i){
        msgStatus += connectSocket(i);
    }
    if (msgStatus!=0) return msgStatus;
    for (uint i = 0; i < ltas_.size(); ++i){
        msgStatus += sendMessage(i, msg, true);
    }
    if (msgStatus!=0) return msgStatus;
    vector<int> indices;
    for (uint i = 0; i < ltas_.size(); ++i){
        indices.push_back(i);
    }
    msgStatus += listenForReply(indices);

    return msgStatus;
}

int masterLta_t::singleSend(int index, const string msg){

    int msgStatus = 0;

    msgStatus += connectSocket(index);
    if (msgStatus!=0) return msgStatus;
    msgStatus += sendMessage(index, msg, false);
    if (msgStatus!=0) return msgStatus;
    vector<int> indices = {index};
    msgStatus += listenForReply(indices);

    return msgStatus;
}

int masterLta_t::connectSocket(int index){
    ltaConfigEntry_t lce = ltas_[index].conf;

    ltas_[index].resp.setCode(lce.code());

    // Sets read timeout (how long to wait for a response)
    struct timeval tv;
    tv.tv_sec = 1; // timeout in seconds
    tv.tv_usec = 0;
    setsockopt(ltas_[index].sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);   

    struct sockaddr_in serv_addr;

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(lce.port()); 

    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, lce.ip().c_str(), &serv_addr.sin_addr)<=0){ 
        printf("Invalid address/ Address not supported \n"); 
        ltas_[index].resp.setSendStatus(1); 
        return 1; 
    } 

    if (connect(ltas_[index].sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){ 
        printf("Connection Failed \n"); 
        ltas_[index].resp.setSendStatus(1); 
        return 1; 
    }
    return 0;
}

int masterLta_t::sendMessage(int index, string msg, bool isBroadcast){
    char * msg_copy = new char[msg.size()+100]; //allocate extra memory for the LTA name
    strcpy(msg_copy, msg.c_str());

    bool msgModified = false;
    //special case: if this is a broadcast and we are sending the "name" command, append the LTA name to the filename
    if (isBroadcast) {
        vector<string> words = tokenize(msg_copy);
        if (words.size()==1 && words[0].compare("multi")==0) {
            sprintf(msg_copy, "multi %s", ltas_[index].conf.code().c_str());
            msgModified = true;
        }
    }

    if (msgModified) {
        send(ltas_[index].sock , msg_copy , strlen(msg_copy) , 0 ); 
    } else {
        send(ltas_[index].sock , msg.c_str() , msg.size() , 0 ); 
    }
    ltas_[index].resp.setSendStatus(0); // send OK 
    free(msg_copy);
    //printf("Message sent\n"); 
    return 0;
}

int masterLta_t::listenForReply(vector<int> indices){

    int valread; 
    int n_sds = indices.size();
    //set up a fd_set with all of the sockets we want to wait on
    fd_set read_sd;
    FD_ZERO(&read_sd);
    int max_sd = -1;
    for (uint i=0; i<indices.size(); i++) {
        int index = indices[i];
        FD_SET(ltas_[index].sock, &read_sd);
        if (ltas_[index].sock>max_sd) max_sd = ltas_[index].sock;
    }

    char buffer[1025] = {0}; //buffer size must be 1 more than the receive size, to allow for null temrination
    while (n_sds>0)
    {
        fd_set rsd = read_sd;
        int sel = select(max_sd+1, &rsd, 0, 0, 0);
        if (sel > 0) {
            for (uint i=0; i<indices.size(); i++) {
                int index = indices[i];
                if (FD_ISSET(ltas_[index].sock, &rsd)) {
                    valread = read( ltas_[index].sock , buffer, 1024); 
                    if (valread > 0) {
                        buffer[valread] = '\0';
                        //LOG_F(INFO, "index %d: %s", index, buffer);
                        ltas_[index].resp.setReplyMsg(ltas_[index].resp.replyMsg().append(buffer));
                        //cout << "valread=" << valread << endl;
                        //cout << buffer << endl;
                    } else if (valread==0) { //connection closed
                        LOG_F(1, "index %d: connection closed", index);
                        ltas_[index].resp.setReplyStatus(0); 
                        FD_CLR(ltas_[index].sock,&read_sd);
                        n_sds--;
                    } else { //read error
                        LOG_F(ERROR, "index %d: read error", index);
                        ltas_[index].resp.setReplyStatus(1);
                        FD_CLR(ltas_[index].sock,&read_sd);
                        n_sds--;
                    }
                }
            }
        } else if (sel == 0) { //timeout
            LOG_F(ERROR, "timeout:");
            for (uint i=0; i<indices.size(); i++) {
                LOG_F(ERROR, "index %d", indices[i]);
                ltas_[indices[i]].resp.setReplyStatus(1);
            }
            break;
        }
    }

    return 0;
}


void masterLta_t::printStatus(){
    stringstream printstream;
    printstream << "\nThe masterLta status is: ";
    switch( status_ ) {
        case eUninitialized:
            printstream << "Uninitialized\n This shouldn't happen.\n";
            break;
        case eOK:
            printstream << "OK\n";
            for (uint i = 0; i < ltas_.size(); ++i){
                printstream << ltas_[i].conf << endl;
            }
            break;
        default:
            printstream << "Error!\n\n";
    }
    LOG_F(INFO, "%s", printstream.str().c_str());
}

int masterLta_t::codeToIndex(const string code) {
    try {
        return code2index_.at(code);
    } catch (const out_of_range& oor) {
        return -1;
    }
}
