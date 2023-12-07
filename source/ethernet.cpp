#include <vector>
#include <sstream>
#include <unistd.h>
#include <cmath>

#include "loguru.hpp" //logging

#include "helperFunctions.h"

#include "ethernet.h"

//#define DELAY_TO_LISTEN_TO_SP_AFTER_SENDING 10000

using namespace std;

bool ethPrintPackets = false;

int Ethernet::listenForBoardResponse(const string &expResp, string &responseBuff, const double workFlowTimeOut)
{
    timespec timeoutTime;
    get_end_time(&timeoutTime, workFlowTimeOut);
    // Get response from the board.
    while (true) {
        string tempBuff;
        recvCmdEthe(tempBuff);
        responseBuff+=tempBuff;
        if (!tempBuff.empty()) LOG_F(1, "received: %s",tempBuff.c_str());
        if (responseBuff.find("ERROR") != string::npos)
        {
            //"ERROR" should break you out of the loop (important for "read" or "runseq")
            LOG_F(WARNING, "possible error: %s", responseBuff.c_str());
            return 2; //halted on LTA error
        }
        if (responseBuff.find(expResp.c_str(),0) != string::npos) //string found
        {
            if (expResp.compare(responseBuff)==0) {
                return 0; //got exactly the expected response
            } else {
                return 3; //got expected response, plus additional messages
            }
        }
        usleep(ETH_LONGWAIT);//we don't expect another string for 10 ms

        if (time_is_up(&timeoutTime)) {
            LOG_F(INFO, "timeout while waiting for \"%s\"", expResp.c_str());
            return 1; //timeout
        }
    }

    return -1; //shouldn't get here
}

int Ethernet::requestStringFromBoard(uint nchar, string &recString) {
    uint32_t blockInt = eUser;
    uint32_t address = ETH_ADDR_SLAVE_DATA;

    // Number of 32-bit words.
    int n = ceil((float)(nchar+3)/4);

    // Buffer length:
    // 2 : r/w flag, # words.
    // 8 : address.
    int length = 2 + 8;
    char * buffer = (char *) malloc(length * sizeof(char));
    unsigned int idx = 0;

    // Read command.
    buffer[idx] = 0;// Read.
    idx++;
    buffer[idx] = n;// # of 64 bits words.
    idx++;

    // Start memory address.
    memcpy(&buffer[idx], &address, sizeof(uint32_t));
    idx += 4;

    memcpy(&buffer[idx], &blockInt, sizeof(uint32_t));
    idx += 4;

    if (ethPrintPackets) LOG_F(2, "request: r/w=%d, n=%d, address=0x%08X (block %d)", buffer[0], buffer[1], address, blockInt);

    int flag = 0;
    while(!flag) 
    {
        // Request data @address to board.
        if (send(fd_, buffer, length, 0) < length)
        {
            LOG_F(ERROR, "Error sending UDP control packet");
            free(buffer);
            return 1;
        }//TODO: if we don't get a response, just re-send the request - is this always safe, or can this result in misaligned requests and responses?

        // Read data back from board.
        sockaddr_in clientAddr;
        unsigned int clientAddrLen = sizeof(clientAddr);

        int recBuff_length = 2 + 8*n + 8; //add extra 8 bytes just in case
        char * recBuff = (char *) malloc(recBuff_length * sizeof(char));

        int numRecBytes = recvfrom(fd_, recBuff, recBuff_length, 0, (sockaddr*)&clientAddr, &clientAddrLen);
        if (numRecBytes == -1)
        {
            LOG_F(WARNING, "UDP error on control port, no message received: %s", strerror(errno));
        }
        else
        {
            flag = 1;
            if (ethPrintPackets) LOG_F(2, "numRecBytes=%d, packType=%d, packCounter=%d", numRecBytes, recBuff[0], (unsigned char) recBuff[1]);
            if (numRecBytes != 2 + 8*n) {
                LOG_F(WARNING, "unexpected packet size %d (expected %d)", numRecBytes, 2 + 8*n);
            }
            int quadWordsCount = (numRecBytes - 2) / 8;

            //allocate an extra byte and null-terminate for safety
            char * strBuff = (char *) malloc((4*quadWordsCount + 1) * sizeof(char));
            strBuff[4*quadWordsCount] = '\0';

            for (int iWord=0; iWord<quadWordsCount; iWord++) {
                uint32_t responseAddr;
                memcpy(&strBuff[4*iWord], &recBuff[2 + 8*iWord], 4);
                memcpy(&responseAddr, &recBuff[2 + 8*iWord + 4], sizeof(uint32_t));
                if (responseAddr<ETH_ADDR_SLAVE_DATA) {
                    LOG_F(WARNING,"unexpected response address 0x%X", responseAddr);
                }
                if (ethPrintPackets) LOG_F(2, "response[%d]: addr 0x%08X, data 0x%02X%02X%02X%02X", iWord, responseAddr, recBuff[2+8*iWord], recBuff[2+8*iWord+1], recBuff[2+8*iWord+2], recBuff[2+8*iWord+3]);
            }
            if (strlen(strBuff) != nchar) {
                LOG_F(WARNING, "received string with length %zu, expected %d", strlen(strBuff), nchar);
            }
            recString.assign(strBuff);
            free(strBuff);
        }
        free(recBuff);
    }
    free(buffer);
    return 0;
}

//read 64 bits from eEth or 32 bits from eUser
int Ethernet::requestWordFromBoard(eEthBlock block, uint32_t address, uint64_t &recData) {
    // Buffer length:
    // 2 : r/w flag, # words.
    // 8 : address.
    int length = 2 + 8;
    char * buffer = (char *) malloc(length * sizeof(char));
    unsigned int idx = 0;

    // Read command.
    buffer[idx] = 0;	// Read.
    idx++;
    buffer[idx] = 1;	// # of 64 bits words.
    idx++;

    // Start memory address.
    memcpy(&buffer[idx], &address, sizeof(uint32_t));
    idx += 4;

    uint32_t blockInt = block;
    memcpy(&buffer[idx], &blockInt, sizeof(uint32_t));
    idx += 4;

    if (ethPrintPackets) LOG_F(2, "request: r/w=%d, n=%d, address=0x%08X (block %d)", buffer[0], buffer[1], address, block);

    uint32_t responseAddr;
    recData = 0;//important to initialize to 0, since the later memcpy may only be 4 bytes
    int flag = 0;
    while(!flag) 
    {
        // Request data @address to board.
        if (send(fd_, buffer, length, 0) < length)
        {
            LOG_F(ERROR, "Error sending UDP control packet");
            free(buffer);
            return 1;
        }//TODO: if we don't get a response, just re-send the request - is this always safe, or can this result in misaligned requests and responses?

        // Read data back from board.
        sockaddr_in clientAddr;
        unsigned int clientAddrLen = sizeof(clientAddr);

        int recBuff_length = 20;
        char * recBuff = (char *) malloc(recBuff_length * sizeof(char));
        int numRecBytes = recvfrom(fd_, recBuff, recBuff_length, 0, (sockaddr*)&clientAddr, &clientAddrLen);
        if (numRecBytes == -1)
        {
            LOG_F(WARNING, "UDP error on control port, no message received: %s", strerror(errno));
        }
        else
        {
            flag = 1;
            if (ethPrintPackets) LOG_F(2, "numRecBytes=%d, packType=%d, packCounter=%d", numRecBytes, recBuff[0], (unsigned char) recBuff[1]);
            if (numRecBytes != 10) {
                LOG_F(WARNING, "unexpected packet size %d (should normally be 10 for a single-word read)", numRecBytes);
            }
            switch (block) {
                case eUser:
                    memcpy(&recData, &recBuff[2], sizeof(uint32_t));
                    memcpy(&responseAddr, &recBuff[2+4], sizeof(uint32_t));
                    if (ethPrintPackets) LOG_F(2, "response: addr 0x%08X, data 0x%08llX", responseAddr, recData);
                    if (responseAddr != address) {
                        LOG_F(WARNING,"response address 0x%X does not match request address 0x%X", responseAddr, address);
                    }
                    break;
                case eEth:
                    memcpy(&recData, &recBuff[2], sizeof(uint64_t));
                    if (ethPrintPackets) LOG_F(2, "response: 0x%016llX", recData);
                    break;
            }
        }
        //clean up
        free(recBuff);
    }

    free(buffer);
    return 0;
}

int Ethernet::sendStringToBoard(const string sendBuff) {
    LOG_F(1, "sending: %s",sendBuff.c_str());
    uint32_t blockInt = eUser;
    uint32_t address = ETH_ADDR_MASTER_DATA;

    // Number of 32-bit words needed to hold the string (including the trailing null).
    int size_w = ceil(((float)(sendBuff.size()+1))/ETH_MEM_WIDTH_BYTES);

    // Buffer length:
    // 2 : r/w flag, # words.
    // 8 : address [63:32]=block, [31:0]=address
    // 8*n : data.
    int length = 2 + 8 + 8*size_w;
    char * buffer = (char *) calloc(length, sizeof(char));//initialize to 0
    unsigned int idx = 0;

    // Write command.
    buffer[idx] = 1; // Write.
    idx++;
    buffer[idx] = size_w; // # of 64 bits words.
    idx++;

    // Start memory address.
    memcpy(&buffer[idx], &address, sizeof(uint32_t));
    idx += 4;

    memcpy(&buffer[idx], &blockInt, sizeof(uint32_t));
    idx += 4;

    // Data.
    for (uint byte_addr=0; byte_addr<sendBuff.size(); byte_addr++) {
        buffer[idx] = sendBuff[byte_addr];
        if ((byte_addr%4)==3) idx +=4; //only fill the bottom 4 bytes of each word
        idx++;
    }

    if (ethPrintPackets) LOG_F(2, "write: r/w=%d, n=%d, address=0x%08X (block %d), value=\"%s\"", buffer[0], buffer[1], address, blockInt, sendBuff.c_str());

    if (send(fd_, buffer, length, 0) < length)
    {
        LOG_F(ERROR, "Error sending UDP control packet");
        free(buffer);
        return 1;
    }
    free(buffer);
    return 0;
}

//write 64 bits of data to eEth or eUser
int Ethernet::sendWordToBoard(eEthBlock block, uint32_t address, uint64_t value) {
    uint32_t blockInt = block;
    // Buffer length:
    // 2 : r/w flag, # words.
    // 8 : address [63:32]=block, [31:0]=address
    // 8 : data.
    int length = 2 + 8 + 8;
    char * buffer = (char *) malloc(length * sizeof(char));
    unsigned int idx = 0;

    // Write command.
    buffer[idx] = 1;	// Write.
    idx++;
    buffer[idx] = 1;	// # of 64 bits words.
    idx++;

    // Start memory address.
    memcpy(&buffer[idx], &address, sizeof(uint32_t));
    idx += 4;

    memcpy(&buffer[idx], &blockInt, sizeof(uint32_t));
    idx += 4;

    // Data.
    memcpy(&buffer[idx], &value,  sizeof(uint64_t));

    if (ethPrintPackets) LOG_F(2, "write: r/w=%d, n=%d, address=0x%08X (block %d), value=0x%016llX", buffer[0], buffer[1], address, block, value);

    if (send(fd_, buffer, length, 0) < length)
    {
        LOG_F(ERROR, "Error sending UDP control packet");
        free(buffer);
        return 1;
    }
    free(buffer);
    return 0;
}

int Ethernet::sendCmdEthe(const string &sendBuff)
{
    // ####################
    // ### Send Command ###
    // ####################

    sendStringToBoard(sendBuff); //send handshake step 1

    uint64_t ack = 0;
    while (true)
    {
        // m_dready start.
        sendWordToBoard(eUser, ETH_ADDR_MASTER_DREADY, ETH_VAL_DREADY_START); //send handshake step 2

        // m_dack start.
        requestWordFromBoard(eUser, ETH_ADDR_MASTER_DACK, ack);//send handshake step 3
        if (ack == ETH_VAL_DACK_START) break;
        usleep(ETH_SHORTWAIT);//wait for eth_mdata_get to run
    }

    ack = 0;
    while (true)
    {	
        // m_dready end.
        sendWordToBoard(eUser, ETH_ADDR_MASTER_DREADY, ETH_VAL_DREADY_END); //send handshake step 4

        // m_dack end.
        requestWordFromBoard(eUser, ETH_ADDR_MASTER_DACK, ack);//send handshake step 5

        if (ack == ETH_VAL_DACK_END) break;

        usleep(ETH_LONGWAIT);//uB waits for 10 ms after setting ETH_ADDR_MASTER_DACK
    }

    return 0;
}

int Ethernet::recvCmdEthe(string &responseBuff)
{
    // ##################
    // ### Get String ###
    // ##################

    // s_dready start.
    // Check if there is a call from the board.
    uint64_t ready = 0;
    requestWordFromBoard(eUser, ETH_ADDR_SLAVE_DREADY, ready);//recv handshake step 1

    // Return if the board is not calling.
    if(ready != ETH_VAL_DREADY_START)
    {
        if (ready != ETH_VAL_DREADY_END)
            LOG_F(WARNING,"unexpected handshake value: ready=0x%08llX", ready);
        return -1;
    }

    // Check the length of the data being sent by the board.
    uint64_t ndata = 0;

    requestWordFromBoard(eUser, ETH_ADDR_SLAVE_DLENGTH, ndata);//recv handshake step 2

    if (ndata>ETH_MAX_DATALENGTH) {
        LOG_F(WARNING,"ndata unreasonably large: 0x%08llX", ndata);
        return -1;
    }

    // Read the data.
    requestStringFromBoard(ndata, responseBuff);//recv handshake step 3

    // s_dack start.
    sendWordToBoard(eUser, ETH_ADDR_SLAVE_DACK, ETH_VAL_DACK_START); //recv handshake step 4

    // s_dready end.
    ready = 0;
    while (true)
    {
        requestWordFromBoard(eUser, ETH_ADDR_SLAVE_DREADY, ready);//recv handshake step 5

        if (ready == ETH_VAL_DREADY_END) break;
        else if (ready != ETH_VAL_DREADY_START)
            LOG_F(WARNING,"unexpected handshake value: ready=0x%08llX", ready);
        usleep(ETH_SHORTWAIT); //wait for eth_sdata_put to check ETH_ADDR_SLAVE_DREADY
    }

    // s_dack end.
    sendWordToBoard(eUser, ETH_ADDR_SLAVE_DACK, ETH_VAL_DACK_END); //recv handshake step 6

    //uB sleeps for 10 ms after it sets ETH_ADDR_SLAVE_DREADY, so calling code should should wait before checking for the next message

    return 0;
}

int Ethernet::configureBoard(ltaBoard* boardPtr)
{
    // Configure board.
    uint64_t data = 1;

    // Set firmware in dynamic mac resolution mode for data mode.
    sendWordToBoard(eEth, 0xB, data);

    // Set IP address and port (for burst data).
    LOG_F(INFO, "Writing burst destination = %s:%d", boardPtr->dataIp().c_str(), boardPtr->dataPort());

    uint64_t ip = 0;
    int ipArr[4];
    sscanf(boardPtr->dataIp().c_str(), "%d.%d.%d.%d",
            &ipArr[3],
            &ipArr[2],
            &ipArr[1],
            &ipArr[0]
          );

    for (int i = 0; i < 4; ++i)
    {
        (((char *)&ip)[i]) += ipArr[i] & 0xFF;
    }

    // Set IP. 
    sendWordToBoard(eEth, 0x6, ip);

    // Set port.
    data = boardPtr->dataPort();
    sendWordToBoard(eEth, 0x8, data);

    // Read configuration back.
    uint64_t recData;
    // Request Burst Destination MAC.
    requestWordFromBoard(eEth, 0x7, recData);

    // TODO: extract MAC from response.
    //cout << "Burst destination MAC = " << recBuff << endl;

    // Request Burst Destination IP.
    requestWordFromBoard(eEth, 0x6, recData);

    // Extract IP address from response buffer.
    stringstream printstream;
    printstream << "Burst destination Address = ";
    for (int i=3; i>=1; i--)
    {
        //printstream << (((int16_t) recBuff[i+2]) & 0xFF) << ".";
        printstream << (((int16_t) (recData >> 8*i)) & 0xFF) << ".";
    }
    printstream << (((int16_t) recData) & 0xFF) << ":";

    // Request Burst Destination Port.
    uint64_t port = 0;	
    requestWordFromBoard(eEth, 0x8, port);

    printstream << port;
    LOG_F(INFO, "%s", printstream.str().c_str());
    //cout << "Burst destination Port = " << recBuff << endl; 

    return 0;

}

//used by ltaSimulator
ctrl_packet unpackCtrlPacket(ssize_t msgLen, char* recvBuff)
{
    ctrl_packet unpackedPacket;
    recvBuff[msgLen] = '\0';//terminate the string just in case
    //initialize everything except data (data is overwritten anyway)
    unpackedPacket.packet_good = true;
    unpackedPacket.write_not_read = recvBuff[0];
    unpackedPacket.n = recvBuff[1];
    unpackedPacket.addr = 0;
    unpackedPacket.addr_h = 0;
    if (unpackedPacket.write_not_read) {
        if (msgLen != 2 + 8*(1+unpackedPacket.n)) {
            LOG_F(WARNING, "wrong number of bytes in this packet!");
            unpackedPacket.packet_good = false;
            return unpackedPacket;
        }
    } else {
        if (msgLen != 2 + 8) {
            LOG_F(WARNING, "wrong number of bytes in this packet!");
            unpackedPacket.packet_good = false;
            return unpackedPacket;
        }
    }
    for (int ibyte=0;ibyte<4;ibyte++) {
        unpackedPacket.addr |= (recvBuff[2+ibyte] << ibyte*8);
    }
    for (int ibyte=0;ibyte<4;ibyte++) {
        unpackedPacket.addr_h |= (recvBuff[2+ibyte+4] << ibyte*8);
    }
    for (int iword=0;iword<unpackedPacket.n;iword++) {
        unpackedPacket.data[iword]=0;
        for (int ibyte=0;ibyte<8;ibyte++) {
            unpackedPacket.data[iword] += (((uint64_t) recvBuff[2+8+ibyte+8*iword] & 0xff) << ibyte*8);
        }
    }
    return unpackedPacket;
}
