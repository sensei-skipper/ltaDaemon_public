#ifndef _ltaSimulator_h_
#define _ltaSimulator_h_

#include <map>
#include <string>
#include <csignal>

#include "dumpToDisc.h"

#define SIMULATOR_SAMPS_PER_PACKET 45
//maximum nesting level in legacy sequencer, set in firmware
#define SEQ_MAXDEPTH 8
//maximum number of smart sequencer recipes, set in firmware
#define SSEQ_NUMRECIPES 100
//110000000000000000000000000 = HD1|HD2
#define PED_STATE 0x2000000
#define SIG_STATE 0x4000000

const int MAXLINE = 1025;
const int delay_ = 0;
//const int delay_ = 10000; //add a delay to test synchronization
//dataBuff is hard-coded to 2000 bytes in dumpToDisc.saveData(), so longer packets will get cut off even if we make the buffer larger here
volatile static std::sig_atomic_t isSending;//0 if not sending, 1 if sending

enum ltaSimMode_t {eNormal, eData, eShutdown};

enum ltaReadType_t {eStandard, eRaw};

enum ltaSeqType_t {eLegacy, eSmart};

typedef struct
{
    udpSocket * socket;
    sockaddr_in * dataAddr;
    ltaReadType_t readType;
    ltaSeqType_t seqType;
    int ch0;
    int nch;
    int sampsToSend;
    int ncol;
    int nsamp;
    float usPerSamp;
    bool printPackets;
}  sendDataArgs_t;

void * sendData(void * args);

class ltaSimulatorArgs
{
    public:
        ltaSimulatorArgs(){parseAddr(CMD_LINE_BOARD_ADDR_DEFAULT, this->boardIP_, this->boardPort_);};

        int processArgs(const int argc, char *argv[]);

        // Getters.
        string boardIP(){return boardIP_;};
        unsigned int boardPort(){return boardPort_;};

    private:
        // Arguments.
        string boardIP_;
        unsigned int  boardPort_;
};

class ltaSimulator_t
{
    public:
        ltaSimulator_t(ltaSimulatorArgs cmdLine);
        ~ltaSimulator_t();
        int mainAction();
    private:
        udpSocket* boardCtrl_;
        ltaSimMode_t mode_;
        string responseBuff_;//text to be sent back to the daemon
        string responseChunk_;//next chunk of text to be sent back
        map<string,string> varMap;//variables for "get" and "set"
        uint64_t registers_[16];//used for network stuff
        uint32_t registers_eth_[100];//used for commands

        int seq_loop_depth_;
        uint32_t seq_loop_n_[SEQ_MAXDEPTH];//loop counts for legacy sequencer
        int seq_loop_depth_ADC_;
        uint32_t seq_sigsamps_;//number of signal samples
        uint32_t seq_pedsamps_;//number of pedestal samples
        uint32_t adcState_;

        int sseq_dynVarValue_;
        int sseq_loop_n_[SSEQ_NUMRECIPES];//loop counts for smart sequencer
        int sseq_mother_[SSEQ_NUMRECIPES];//mother IDs for smart sequencer
        int sseq_adcRecipe_;//the ID of the recipe where the ADC is active

        float usPerSamp_;
        bool printPackets_;
        sendDataArgs_t * dataArgs_;

        pthread_t dataThread;

        int doReadout();
        int parseMsgAndProduceResponse(ssize_t msgLen, char msg[], string &response);
        int moveTextToTransfer();
        int listenForControlPacket();
        int sendWriteToDaemon(const struct sockaddr_in &toAddr, uint64_t response);
        int sendCmdResponseEthe(const struct sockaddr_in &boardAddr, const string &sendBuff, int n);
};

#endif
