#ifndef _ethernet_h_
#define _ethernet_h_

#include <string.h>

#include "baseSocket.h"
#include "ltaBoard.h"

#define ETH_MEM_WIDTH_BYTES 4
#define ETH_MAX_DATALENGTH 256 //max length of a string sent or received

#define ETH_ADDR_MASTER_DREADY 0x0 //place in the ethernet memory that is used by the master (soft) to indicate the slave (board) that the data is ready
#define ETH_ADDR_MASTER_DATA 0x6 //place in the ethernet memory that is used by the master to copy the message to the slave
#define ETH_ADDR_MASTER_DACK 0x1 //place in the ethernet memory that is used by the slave(board) to indicate the master (soft) that is taking the data

#define ETH_VAL_DREADY_START 0x78787878 //memory value to indicate data is ready to be read by the uB
#define ETH_VAL_DREADY_END 0xCDCDCDCD //memory value to indicate to the uB that is data transmition is ending form the master end
#define ETH_VAL_DACK_START 0xABABABAB //memory value to put by slave to indicate to the master that is processing the message
#define ETH_VAL_DACK_END 0xEFEFEFEF //memory value to put by the slave to indicate to the master that finished processing the message

#define ETH_ADDR_SLAVE_DREADY 0x3 //place in the ethernet memory where that is used by the master to indicate the slave that the data is ready
#define ETH_ADDR_SLAVE_DLENGTH 0x5 //place in the ethernet memory whith the number of bytes being sent by the board as master to the soft
#define ETH_ADDR_SLAVE_DATA 70 //place in memory where the master (board) copies the data to send to the slave (soft)
#define ETH_ADDR_SLAVE_DACK 0x4 //place in the ethernet memory that is used by the software when slave to indicate the master(board) that the data has being collected

#define ETH_SHORTWAIT 1000 //1 ms wait, for when we don't know when the LTA will be ready for us
#define ETH_LONGWAIT 5000 //5 ms wait, for when we don't expect the LTA to be ready for the next 10 ms

using namespace std;

typedef struct
{
    bool packet_good;
    bool write_not_read;
    unsigned char n; //number of data words in a write packet, number of requested words in a read packet
    unsigned int addr; //the register address is the low half of the address word
    unsigned int addr_h; //the high half of the address word is 1 for register accesses, 0 for terminal?
    uint64_t data[256]; //n data words (only used for write packet)
}  ctrl_packet;

enum eEthBlock {eUser=0, eEth=1}; //ots_block_sel in ethernet_interface.vhd


class Ethernet
{
    public:
        Ethernet(int fd): fd_(fd){};
        ~Ethernet(){};
        int listenForBoardResponse(const string &expResp, string &responseBuff, const double workFlowTimeOut);
        int recvCmdEthe(string &responseBuff);
        int sendCmdEthe(const string &sendBuff);
        int configureBoard(ltaBoard* boardPtr);

    private:
        int fd_;
        int requestStringFromBoard(uint nchar, string &recString);
        int requestWordFromBoard(eEthBlock block, uint32_t address, uint64_t &recData);
        int sendStringToBoard(const string sendBuff);
        int sendWordToBoard(eEthBlock block, uint32_t address, uint64_t value);
};


ctrl_packet unpackCtrlPacket(ssize_t msgLen, char* recvBuff);

#endif
