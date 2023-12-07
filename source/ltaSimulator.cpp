#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <math.h>

#include<sys/socket.h>  //socket
#include<arpa/inet.h>   //inet_addr
#include <sys/time.h>   //timeval

#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <vector>
#include <sys/file.h>
#include <streambuf>

#include <termios.h> /* POSIX terminal control definitions */
#include <thread>
#include <pthread.h>

#include "loguru.hpp" //logging
#include <limits.h> //PATH_MAX

#include "sequencerHandler.h"
#include "ethernet.h"
#include "commandLineArgs.h"
#include "udpSocket.h"
#include "dataConversion.h"
#include "userInterpreter.h"
#include "ltaSimulator.h"

using namespace std;

int ltaSimulatorArgs::processArgs(const int argc, char *argv[])
{
    int opt = 0;

    while ( (opt = getopt(argc, argv, "hb:")) != -1 )
    {
        int errStatus = 0;
        switch (opt)
        {
            case 'h':
                cout << "ltaSimulator.exe <command>" << endl;
                cout << "\t-b <ip:port>: address of simulated LTA's control port (default " << CMD_LINE_BOARD_ADDR_DEFAULT << ")" << endl;
                return 1;
            case 'b':
                errStatus = parseAddr(optarg, this->boardIP_, this->boardPort_);
                if( errStatus!=0 ){
                    LOG_F(ERROR, "Problem parsing board address");
                    return -1;
                } 
                break;
        }
    }
    return 0;
}

ltaSimulator_t::ltaSimulator_t(ltaSimulatorArgs cmdLine)
{
    mode_ = eNormal;
    boardCtrl_ = new udpSocket(cmdLine.boardIP(), "simulator control", cmdLine.boardPort());
    boardCtrl_->create();
    for (int i=0;i<16;i++) registers_[i] = 0;
    for (int i=0;i<100;i++) registers_eth_[i] = 0;
    usPerSamp_ = 1.0; //we typically have ~50 us/sample in real life
    printPackets_ = false;
    dataArgs_ = (sendDataArgs_t *)malloc(sizeof(sendDataArgs_t));

    varMap["cdsout"] = "2";//default to "signal - pedestal"

    struct sockaddr_in * dataAddr;
    dataAddr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in)); 
    memset(dataAddr, '0', sizeof(struct sockaddr_in)); 
    dataAddr->sin_family = AF_INET; 
    dataArgs_->dataAddr = dataAddr;
    dataArgs_->ch0 = 12;
    dataArgs_->nch = LTA_NUMBER_OF_CHANNELS;
    registers_eth_[ETH_ADDR_SLAVE_DREADY] = ETH_VAL_DREADY_END;//no text for the daemon to read
}

ltaSimulator_t::~ltaSimulator_t()
{
    free(dataArgs_->dataAddr);
    free(dataArgs_);
    delete boardCtrl_;
}

int ltaSimulator_t::parseMsgAndProduceResponse(ssize_t msgLen, char msg[], string &response){
    response.clear();
    string msgOrig = msg;
    char *commandWord[10];
    char *token;
    char *rest;
    rest = msg;
    int wordInd = 0;
    while( (token = strtok(rest, " \r\n")) ) {
        rest = NULL;
        commandWord[wordInd] = token;
        //cout << "word[" << wordInd << "]=\"" << commandWord[wordInd] << "\"" << endl;
        wordInd++;
    }
    uint32_t val;
    uint8_t indSeq; //sequencer memory is 256 32-bit words
    uint8_t opcodeSeq;
    uint32_t valSeq;
    if (strcmp(commandWord[0],"sek")==0) {
        if (strcmp(commandWord[1],"clear")==0) {
            if (strcmp(commandWord[2],"recipes")==0) {
                //sek clear recipes
                sseq_adcRecipe_ = -1;
                for (int i=0; i<SSEQ_NUMRECIPES; i++) {
                    sseq_loop_n_[i] = -1;
                    sseq_mother_[i] = -1;
                }
            } else if (strcmp(commandWord[2],"dynamicVector")==0) {
                //sek clear dynamicVector
                sseq_dynVarValue_ = -1;
            }
        } else if (strcmp(commandWord[1],"recipe")==0) {
            int iRecipe = strtol(commandWord[2],0,10);
            if (strcmp(commandWord[3],"status")==0) {
                val = strtol(commandWord[5],0,10);
                if (val & 0x6000000) {//ADC is active
                    sseq_adcRecipe_ = iRecipe;
                }
            } else if (strcmp(commandWord[3],"n")==0) {
                sseq_loop_n_[iRecipe] = atoi(commandWord[4]);
            } else if (strcmp(commandWord[3],"dynamic")==0) {
                //assume a single-valued dynamic variable
                sseq_loop_n_[iRecipe] = sseq_dynVarValue_;
            } else if (strcmp(commandWord[3],"mother")==0) {
                sseq_mother_[iRecipe] = atoi(commandWord[4]);
            }
        } else if (strcmp(commandWord[1],"dynamicVector")==0) {//dynamic variable definition
            if (strcmp(commandWord[2],"0")==0) //only handle a single dynamic variable
                sseq_dynVarValue_ = strtol(commandWord[3],0,10); //assume a single-valued dynamic variable
        } else if (strcmp(commandWord[1],"start")==0) {
            dataArgs_->readType = eStandard;
            dataArgs_->seqType = eSmart;
            doReadout();//start standard readout, smart sequencer
        }
    } else switch (wordInd) {
        case 1:
            if (strcmp(commandWord[0],LTASOFT_STOPSIM_WORD)==0) {
                LOG_F(INFO, "stop requested, shutting down simulator");
                mode_ = eShutdown;
            }
            break;
        case 2:
            if (strcmp(commandWord[0],"get")==0) {
                if (strcmp(commandWord[1],"all")==0) {
                    for (map<string,string>::iterator it=varMap.begin(); it!=varMap.end(); ++it) {
                        response.append(it->first + " = " + it->second + "\r\n");
                    }
                } else {
                    response.append(commandWord[1]);
                    response.append(" = ");
                    response.append(varMap[commandWord[1]]);
                    response.append("\r\n");
                }
            }
            break;
        case 3:
            if (strcmp(commandWord[0],"set")==0) {
                varMap[commandWord[1]] = commandWord[2];
                if (strcmp(commandWord[1],"packStart")==0 && strcmp(commandWord[2],"1")==0) {
                    //we could use this variable to distinguish between a "read" and a "runseq"
                } else if (strcmp(commandWord[1],"seqStart")==0 && strcmp(commandWord[2],"1")==0) {
                    dataArgs_->readType = eStandard;
                    dataArgs_->seqType = eLegacy;
                    doReadout();//start standard readout
                } else if (strcmp(commandWord[1],"bufTraStart")==0 && strcmp(commandWord[2],"1")==0) {
                    dataArgs_->readType = eRaw;
                    dataArgs_->seqType = eLegacy;
                    doReadout();//start raw readout
                } else if (strcmp(commandWord[1],"simUsPerSamp")==0) {
                    usPerSamp_ = atof(commandWord[2]);
                } else if (strcmp(commandWord[1],"seq")==0 && strcmp(commandWord[2],"clear")==0) {
                    seq_loop_depth_ = 0;//reset the loop depth
                    seq_loop_depth_ADC_ = -1;
                    seq_sigsamps_ = 0;
                    seq_pedsamps_ = 0;
                    for (int i=0; i<SEQ_MAXDEPTH; i++) {
                        seq_loop_n_[i] = -1;
                    }
                }
            } else if (strcmp(commandWord[0],"get")==0) {
                if (strcmp(commandWord[1],"telemetry")==0) {
                    if (strcmp(commandWord[2],"all")==0) {
                        for (map<string,string>::iterator it=varMap.begin(); it!=varMap.end(); ++it) {
                            response.append(it->first + "tele=" + it->second + "\r\n");
                        }
                    }
                }
            }
            break;
        case 4:
            if (strcmp(commandWord[0],"set")==0) {
                if (strcmp(commandWord[1],"seq")==0) {
                    indSeq = atoi(commandWord[2]);
                    val = strtol(commandWord[3],0,10);
                    opcodeSeq = val >> numOfBitsValue;
                    valSeq = val & ((1 << numOfBitsValue) - 1); //29 bits
                    //response << "sequencer[" << dec << indSeq << "]=0x" << hex << val << "\r\n";

                    if (opcodeSeq==delayOC.to_ulong()) {
                        LOG_F(1,"sequencer[%d]: delay, n=%d\n",indSeq,valSeq); //delay in clocks
                    } else if (opcodeSeq==statusOC.to_ulong()) {
                        LOG_F(1,"sequencer[%d]: status, state=0x%x\n",indSeq,valSeq); //values of output signals
                        uint32_t newAdcState = valSeq & (PED_STATE | SIG_STATE);
                        if (newAdcState!=0 && newAdcState!=adcState_) {//ADC is active
                            if (seq_loop_depth_ADC_==-1) {
                                seq_loop_depth_ADC_ = seq_loop_depth_;
                                LOG_F(1, "found ADC readout at loop level %d", seq_loop_depth_);
                            } else {
                                if (seq_loop_depth_ADC_!=seq_loop_depth_) {
                                    LOG_F(WARNING, "ADC is active at multiple loop depths, simulator may not be able to guess the image dimensions");
                                }
                            }
                            if (valSeq & PED_STATE) {//pedestal integration
                                seq_pedsamps_++;
                            }
                            if (valSeq & SIG_STATE) {//signal integration
                                seq_sigsamps_++;
                            }
                        }
                        adcState_ = newAdcState;
                    } else if (opcodeSeq==loopOC.to_ulong()) {
                        LOG_F(1,"sequencer[%d]: loop, n=%d\n",indSeq,valSeq); //number of loops, minus 1
                        seq_loop_n_[seq_loop_depth_++] = valSeq;
                        adcState_ = 0;//clear ADC state
                    } else if (opcodeSeq==endLoopOC.to_ulong()) {
                        LOG_F(1,"sequencer[%d]: endLoop, n=%d\n",indSeq,valSeq); //n should be the index of the loop instruction, plus 1
                        seq_loop_depth_--;
                        adcState_ = 0;//clear ADC state
                    } else if (opcodeSeq==endSeqOC.to_ulong()) {
                        LOG_F(1,"sequencer[%d]: endSeq, n=%d\n",indSeq,valSeq); //n should be 0
                    }
                }
            }
            break;
    }
    /*
    if (generic_response) {
        response.append("MESSAGE: ");
        response.append(msgOrig);
        response.append("\r\n");
    }
    */
    response.append(LTA_GEN_DONE_MESSAGE);
    LOG_F(1, "RESPONSE: %s", response.c_str());

    return 0;
}

int ltaSimulator_t::moveTextToTransfer() {
    const int charsToTransfer = 20;//how many characters to transfer at a time

    responseChunk_.assign(responseBuff_.substr(0,charsToTransfer));
    responseBuff_.erase(0,charsToTransfer);
    //cout <<"responseBuff:" << endl << responseBuff << endl;
    //cout <<"responseChunk:" << endl << responseChunk << endl;
    registers_eth_[5] = responseChunk_.length();//recv handshake step 2//TODO: 5 is hard-coded in recvCmdEthe
    registers_eth_[ETH_ADDR_SLAVE_DREADY] = ETH_VAL_DREADY_START;//recv handshake step 1
    return 0;
}

int ltaSimulator_t::doReadout() {
    //initialize struct with data generator arguments
    dataArgs_->socket = boardCtrl_;
    //initialize daemon address
    dataArgs_->dataAddr->sin_addr.s_addr = ((in_addr_t) ntohl(registers_[6]));
    dataArgs_->dataAddr->sin_port = htons(registers_[8]); 

    mode_ = eData;
    LOG_F(INFO, "ready to send data");

    int nrow = 1;
    int ncol = 1;
    int nsamp = 1;
    if (dataArgs_->seqType==eLegacy) {
        nrow = seq_loop_n_[seq_loop_depth_ADC_ - 3] + 1;
        ncol = seq_loop_n_[seq_loop_depth_ADC_ - 2] + 1;
        nsamp = seq_loop_n_[seq_loop_depth_ADC_ - 1] + 1;
        switch (atoi(varMap["cdsout"].c_str())) {
            case 0://pedestal
                nsamp *= seq_pedsamps_;
                break;
            case 1://signal
                nsamp *= seq_sigsamps_;
                break;
            case 2://signal-pedestal
            case 3://pedestal-signal
                if (seq_pedsamps_ != seq_sigsamps_) {
                    LOG_F(WARNING, "%d signal samples != %d pedestal samples: should be equal for cdsout=%s",seq_sigsamps_, seq_pedsamps_, varMap["cdsout"].c_str());
                }
                nsamp *= seq_sigsamps_;
                break;
        }
        dataArgs_->sampsToSend = nrow * ncol * nsamp;
    } else {//eSmart
        nsamp = sseq_loop_n_[sseq_adcRecipe_];
        int horzRecipe = sseq_mother_[sseq_adcRecipe_];
        ncol = sseq_loop_n_[horzRecipe];
        int vertRecipe = sseq_mother_[horzRecipe];
        nrow = sseq_loop_n_[vertRecipe];
        dataArgs_->sampsToSend = nrow * ncol * nsamp;
    }
    dataArgs_->ncol = ncol;
    dataArgs_->nsamp = nsamp;
    dataArgs_->usPerSamp = usPerSamp_;
    dataArgs_->printPackets = printPackets_;
    LOG_F(INFO, "%d channels, total samples per channel %d, nrow %d, ncol %d, nsamp %d", dataArgs_->nch, dataArgs_->sampsToSend, nrow, dataArgs_->ncol, dataArgs_->nsamp);
    //start data generator
    isSending = 1;
    pthread_create(&dataThread, NULL, sendData, (void*) dataArgs_);

    return 0;
}

int ltaSimulator_t::listenForControlPacket() {
    // Address (IP and port) from which the LTA gets control packets. Responses should be sent to the same address.
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);

    char recvBuff[MAXLINE];
    //usleep(1000);//wait, then do a non-blocking read with MSG_DONTWAIT - this doesn't work unless you make the wait shorter than the client-side timeout
    //ssize_t msgLen = recvfrom(boardCtrl->fd(), (char *)recvBuff, MAXLINE, MSG_DONTWAIT, ( struct sockaddr *) &cliaddr, &len);
    ssize_t msgLen = recvfrom(boardCtrl_->fd(), (char *)recvBuff, MAXLINE, 0, ( struct sockaddr *) &cliaddr, &len);
    if(msgLen>0){
        //cout << "addr: " << ntohl((uint32_t) cliaddr.sin_addr.s_addr) << " port: " << ntohs((uint16_t) cliaddr.sin_port) << endl;
        if (delay_>0)
            usleep(delay_);
        //cout << msgLen << " bytes received:" << endl;
        //for (int i=0;i<msgLen;i++) {
        //cout << hex << (+recvBuff[i] & 0xff) << " ";
        //}
        //cout << dec << endl;
        ctrl_packet msgUnpacked = unpackCtrlPacket(msgLen, recvBuff);

        stringstream printstream;
        printstream << (msgUnpacked.write_not_read?"write":"read") << " n=" << +msgUnpacked.n;
        printstream << hex << " addr=" <<msgUnpacked.addr;
        printstream << hex << " addr_h=" <<msgUnpacked.addr_h;

        int status;
        char m_data_char[MAXLINE];
        if (msgUnpacked.write_not_read) {
            for (int iword=0; iword<msgUnpacked.n; iword++)
                printstream << hex << " data=" << msgUnpacked.data[iword] << dec;
            if (msgUnpacked.addr_h == 1) { //TODO: indicates register write?
                registers_[msgUnpacked.addr] = msgUnpacked.data[0];
                printstream << dec << endl;
            } else switch (msgUnpacked.addr) {
                case ETH_ADDR_MASTER_DATA: //software command, send handshake step 1
                    for (int iword=0; iword<msgUnpacked.n; iword++) {
                        for (int ibyte=0;ibyte<4;ibyte++) {
                            m_data_char[4*iword+ibyte] = (msgUnpacked.data[iword] >> 8*ibyte) & 0xFF;
                        }
                    }
                    LOG_F(1, " command string: %s", m_data_char);
                    status = parseMsgAndProduceResponse(msgLen, m_data_char, responseBuff_);
                    if (status!=0) {
                        return status;
                    }
                    //pull out a chunk of text
                    moveTextToTransfer();
                    break;
                case ETH_ADDR_MASTER_DREADY:
                    printstream << hex << msgUnpacked.data[0] << dec << endl;
                    if (msgUnpacked.data[0] == ETH_VAL_DREADY_START) {//send handshake step 2
                        registers_eth_[ETH_ADDR_MASTER_DACK] = ETH_VAL_DACK_START;//send handshake step 3
                    } else if (msgUnpacked.data[0] == ETH_VAL_DREADY_END) {//send handshake step 4
                        registers_eth_[ETH_ADDR_MASTER_DACK] = ETH_VAL_DACK_END;//send handshake step 5
                    }
                    break;
                case ETH_ADDR_SLAVE_DACK:
                    printstream << hex << msgUnpacked.data[0] << dec << endl;
                    if (msgUnpacked.data[0] == ETH_VAL_DACK_START) {//recv handshake step 4
                        registers_eth_[ETH_ADDR_SLAVE_DREADY] = ETH_VAL_DREADY_END;//recv handshake step 5
                    }
                    else if (msgUnpacked.data[0] == ETH_VAL_DACK_END) {//recv handshake step 6
                        printstream << "end of recv handshake" << endl;
                        responseChunk_.clear();
                        if (!responseBuff_.empty())
                            moveTextToTransfer();
                        else {
                            LOG_F(1, "done sending command response");
                            if (mode_==eShutdown) {
                                LOG_F(INFO, "terminating");
                                return -1;
                            }
                        }
                    }
                    break;
                default:
                    LOG_F(WARNING, "unrecognized write address: %d", msgUnpacked.addr);
            }
        } else {//read
            printstream << dec << endl;
            string sendBuff;
            if (msgUnpacked.addr_h == 1) { //TODO: indicates register read?
                //cout << hex << "response: registers[" << msgUnpacked.addr << "]=" << registers[msgUnpacked.addr] << dec << endl;
                sendWriteToDaemon(cliaddr, registers_[msgUnpacked.addr]);
            } else { //terminal protocol?
                if (msgUnpacked.addr == ETH_ADDR_SLAVE_DATA) {//special case: reading command response
                    printstream << "response text: " << responseChunk_ << endl;
                    sendCmdResponseEthe(cliaddr, responseChunk_, msgUnpacked.n);//recv handshake step 3
                } else {
                    //cout << hex << "response: registers_eth[" << msgUnpacked.addr << "]=" << registers_eth[msgUnpacked.addr] << dec << endl;
                    uint64_t response = 0;
                    response += ((uint64_t) msgUnpacked.addr << 32);
                    response += registers_eth_[msgUnpacked.addr];
                    sendWriteToDaemon(cliaddr, response);
                }
            }
        }
        if (printPackets_)
            LOG_F(INFO, "%s", printstream.str().c_str());
    }

    // cout << '.' <<flush;
    return 0;
}

int ltaSimulator_t::sendWriteToDaemon(const struct sockaddr_in &toAddr, uint64_t response) {

    // Buffer length:
    // 2 : r/w flag, # words.
    // 8 : data.
    int length = 2 + 8;
    char * buffer = (char *) malloc(length * sizeof(char));
    unsigned int idx = 0;

    // Write command.
    buffer[idx] = 32; // magic number from ethernet block? daemon doesn't care
    idx++;
    buffer[idx] = 0; // packet counter? daemon doesn't care
    idx++;

    // Start memory address.
    memcpy(&buffer[idx], &response, sizeof(uint64_t));
    idx += 8;

    if (printPackets_) LOG_F(1, "response: packType=%d, packCounter=%d, value=0x%016llX", buffer[0], buffer[1], response);

    if (sendto(boardCtrl_->fd(), buffer, length, 0, (struct sockaddr *)&toAddr, sizeof(sockaddr_in)) < length)
    {
        LOG_F(ERROR, "Error writing buffer for port ");
        free(buffer);
        return 1;
    }
    free(buffer);
    return 0;
}

int ltaSimulator_t::sendCmdResponseEthe(const struct sockaddr_in &toAddr, const string &sendBuff, int n)
{
    // Number of 32-bit words.
    //int size_w = ceil(((float)(sendBuff.size()+1))/ETH_MEM_WIDTH_BYTES);
    uint32_t address = ETH_ADDR_SLAVE_DATA;

    // Buffer length:
    // 2 : r/w flag, # words.
    // 8*n : data.
    int length = 2 + 8*n;
    char * buffer = (char *) calloc(length, sizeof(char));//initialize to 0
    unsigned int idx = 0;

    // Write command.
    buffer[idx] = 32; // magic number from ethernet block? daemon doesn't care
    idx++;
    buffer[idx] = 0; // packet counter? daemon doesn't care
    idx++;

    // Data.
    for (int i=0; i<n; i++)
    {
        for (int ibyte=0;ibyte<4;ibyte++) {
            uint byte_addr = ibyte+ETH_MEM_WIDTH_BYTES*i;
            if (byte_addr < sendBuff.length())
                buffer[idx + ibyte] = sendBuff.at(byte_addr);
        }
        memcpy(&buffer[idx+4], &address, sizeof(uint32_t));
        idx += 8;
    }

    if (printPackets_) LOG_F(1, "response: packType=%d, packCounter=%d, value=\"%s\"", buffer[0], buffer[1], sendBuff.c_str());

    //intentionally send an unterminated string AAAA for testing
    //if (sendBuff.find(LTA_GEN_READ_DONE_MESSAGE) != string::npos) for (int i=0;i<length;i++) buffer[i]=65;
    //intentionally send an empty string for testing
    //if (sendBuff.find(LTA_GEN_READ_DONE_MESSAGE) != string::npos) for (int i=0;i<length;i++) buffer[i]=0;

    if (sendto(boardCtrl_->fd(), buffer, length, 0, (struct sockaddr *)&toAddr, sizeof(sockaddr_in)) < length)
    {
        LOG_F(ERROR, "Error writing buffer for port ");
        free(buffer);
        return 1;
    }
    free(buffer);

    return 0;
}

int ltaSimulator_t::mainAction() {
    if (mode_ == eData) {
        if (!isSending) {
            pthread_join(dataThread, NULL);
            mode_ = eNormal;
            switch (dataArgs_->readType) {
                case eStandard:
                    responseBuff_.append(LTA_GEN_READ_DONE_MESSAGE);
                    moveTextToTransfer();
                    break;
                case eRaw:
                    responseBuff_.append(LTA_GEN_RAW_TRANSFER_DONE_MESSAGE);
                    moveTextToTransfer();
                    break;
            }
            LOG_F(INFO, "done sending data");
        }
    }
    return listenForControlPacket();
}

void * sendData(void * args) {
    sendDataArgs_t * dataArgs = (sendDataArgs_t *) args;
    udpSocket * socket = dataArgs->socket;
    sockaddr_in * dataAddr = dataArgs->dataAddr;
    int sampsToSend = dataArgs->sampsToSend;
    int ch0 = dataArgs->ch0;
    int nch = dataArgs->nch;
    int ncol = dataArgs->ncol;
    int nsamp = dataArgs->nsamp;
    float usPerSamp = dataArgs->usPerSamp;
    bool printPackets = dataArgs->printPackets;

    loguru::set_thread_name("sendData thread");

    LOG_F(INFO, "sending data, ncol=%d, nsamp=%d, nch=%d, sampsToSend=%d", ncol, nsamp, nch, sampsToSend);

    int sampsSent = 0;//used to track pixel addresses

    int packCounter = 0;
    int maxCountValue = 15;
    uint8_t packType = 0;
    int counterValue = 0;
    char buffer[LTA_SOFT_DATABUFF_SIZE];
    while (sampsToSend>0) {
        int sampsThisPacket = SIMULATOR_SAMPS_PER_PACKET;
        if (sampsToSend<sampsThisPacket) {
            sampsThisPacket = sampsToSend;
        }
        sampsToSend -= sampsThisPacket;

        //standard:
        //packType byte, not used?
        //packCounter byte, not used?
        //some number of 64-bit words:
        //count = [63 downto 60], increments each word, rolls over
        //id = [59 downto 56], channel ID, each channel goes to its own FITS file
        //data = [31 downto 0], 32-bit signed int

        //raw:
        //packType byte, not used?
        //packCounter byte, not used?
        //some number of 64-bit words:
        //count = [63 downto 60], increments each word, rolls over
        //id = [59 downto 56], channel ID, each channel goes to its own FITS file
        //header = [55 downto 54], purpose unknown
        //data = [17 downto 0], 18-bit signed int

        int nBytes = 0;

        buffer[nBytes++] = packType;
        buffer[nBytes++] = packCounter;
        for (int iPixel=0;iPixel<sampsThisPacket;iPixel++) {
            //int iSamp = sampsSent % nsamp;
            int iCol = (sampsSent/nsamp) % ncol;
            int iRow = (sampsSent/nsamp)/ncol;
            for (int iCh=ch0;iCh<ch0+nch;iCh++) {
                uint8_t id_byte = iCh;
                id_byte += ((counterValue & 0xF) << 4);
                int data;
                uint8_t header;
                switch (dataArgs->readType) {
                    case eStandard:
                        data = ((iCol % 100) > (iRow % 100))?100:-100;
                        data += iCh;
                        for (int j=0;j<4;j++) {
                            buffer[nBytes++] = ((data >> 8*j) & 0xFF);
                        }
                        buffer[nBytes++] = 0;
                        buffer[nBytes++] = 0;
                        buffer[nBytes++] = 0;
                        buffer[nBytes++] = id_byte;
                        break;
                    case eRaw:
                        header = 1;
                        data = ((iCol % 100) > (iRow % 100))?100:-100;
                        if (data < 0) data += pow(2, ADC_NUM_BITS);//two's complement
                        for (int j=0;j<3;j++) {
                            buffer[nBytes++] = ((data >> 8*j) & 0xFF);
                        }
                        buffer[nBytes++] = 0;
                        buffer[nBytes++] = 0;
                        buffer[nBytes++] = 0;
                        buffer[nBytes++] = header << 6;
                        buffer[nBytes++] = id_byte;
                        break;
                }

                counterValue++;
                if (counterValue>maxCountValue) counterValue = 0;

                CHECK_LE_F(nBytes, LTA_SOFT_DATABUFF_SIZE, "packet is longer than the daemon receive buffer");
            }
            sampsSent++;
        }

        packCounter++;
        if (printPackets)
            LOG_F(INFO, "data packet sent, %d samps left", sampsToSend);
        sendto(socket->fd(), buffer, nBytes, 0, (struct sockaddr *)dataAddr, sizeof(sockaddr_in));
        usleep((int) (usPerSamp * sampsThisPacket));
    }
    isSending = 0;
    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    loguru::init(argc, argv);
    loguru::set_thread_name("ltaSimulator");
    char log_path[PATH_MAX];
    loguru::suggest_log_path("logs", log_path, sizeof(log_path));
    loguru::add_file(log_path, loguru::Append, loguru::Verbosity_MAX);

    // Get arguments from command line.
    ltaSimulatorArgs cmdLine;
    int returnCode = cmdLine.processArgs( argc, argv);
    if (returnCode != 0)
    {
        if (returnCode<0) LOG_F(ERROR, "Error processing command line arguments!!");
        return returnCode;
    }

    ltaSimulator_t* ltaSim = new ltaSimulator_t(cmdLine);

    int status = 0;
    // Main Loop.
    while (status == 0) {
        status = ltaSim->mainAction();
    }

    delete ltaSim;
    return 0;
}
