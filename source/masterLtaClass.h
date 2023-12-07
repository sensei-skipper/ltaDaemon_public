#ifndef _masterLtaClass_h_
#define _masterLtaClass_h_

#include <map>
#include <string>

enum ltaMode_t { eMaster=0, eSlave=1};
enum masterLtaStatus_t { eOK=0, eError=1, eUninitialized=2};

class ltaReply_t
{
    private: 
        string code_;
        string ID_;
        int sendStatus_;
        int replyStatus_;
        string replyMsg_;

    public:
        ltaReply_t(string code="none", string ID="none", int sendStatus=-1, int replyStatus=-1, string replyMsg=""): code_(code), ID_(ID), sendStatus_(sendStatus), replyStatus_(replyStatus), replyMsg_(replyMsg){};
        ~ltaReply_t(){};
        friend ostream & operator << (ostream &out, const ltaReply_t &c); 
        string code()     const { return code_;};
        string ID()       const { return ID_;};
        int    sendStatus()   const { return sendStatus_;};
        int    replyStatus()  const { return replyStatus_;};
        string replyMsg()     const { return replyMsg_;};
        void   setSendStatus (int sendStatus)  { sendStatus_ = sendStatus;};
        void   setReplyStatus(int replyStatus) { replyStatus_= replyStatus;};
        void   setReplyMsg(string replyMsg)    { replyMsg_   = replyMsg;};
        void   setCode(string code)            { code_   = code;};
        void   setID(string ID)                { ID_   = ID;};
};

class ltaConfigEntry_t 
{ 
    private: 
        string code_, ip_;
        int port_;
    public: 
        ltaConfigEntry_t() : code_(), ip_(), port_(){};
        ltaConfigEntry_t(string code, string ip, int port) : code_(code), ip_(ip), port_(port){};
        friend ostream & operator << (ostream &out, const ltaConfigEntry_t &c); 
        string code() const {return code_;};
        string ip()   const {return ip_;};
        int    port() const {return port_;};
}; 

typedef struct
{
    ltaConfigEntry_t conf;
    int sock;
    ltaReply_t resp;
}  lta_t;

class masterLta_t
{
    public:
        masterLta_t(const std::string &confFile);
        ~masterLta_t(){};
        int broadcast(const string msg);
        int singleSend(int index, const string msg);
        void printStatus();
        masterLtaStatus_t getStatus(){return status_;};
        lta_t getLta(int index){return ltas_[index];};
        int codeToIndex(const string code);
        int getN(){return ltas_.size();};
    private:
        int connectSocket(int index);
        int sendMessage(int index, string msg, bool isBroadcast);
        int listenForReply(vector<int> indices);
        int readLtaConfFile(const string &confFile);
        std::vector<lta_t> ltas_;
        std::map<string, int> code2index_;
        masterLtaStatus_t status_;
};

#endif
