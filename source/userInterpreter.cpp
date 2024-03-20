#include <vector>
#include <string.h>
#include <iostream>
#include <sstream>
#include <unistd.h> //close (on OS X)
#include <libgen.h> //basename/dirname
#include <errno.h>

#include <algorithm> //transform

#include "loguru.hpp" //logging

#include "userInterpreter.h"
#include "ethernet.h"
#include "dumpToDisc.h"
#include "dataConversion.h"
#include "sequencerHandler.h"
#include "smartSequencerHandler.h"
#include "helperFunctions.h"

#define LTASOFT_READOUT_TYPE_STANDARD 0
#define LTASOFT_READOUT_TYPE_INTERLEAVING 1
#define LTASOFT_READOUT_TYPE_INTERLEAVING_CALIBRATE 2

#define LTASOFT_CALIBRATION_NUMBER_COLS 100
#define LTASOFT_CALIBRATION_NUMBER_ROWS 200
#define LTASOFT_CALIBRATION_NUMBER_PIXELS_SAFE_GOOD_REGION 30
#define LTASOFT_CALIBRATION_NUMBER_NSAMP 1

#define SP_WORK_FLOW_TIMEOUT 100//maximum time waiting for a complete answere of the board, in sec
//#define SP_REQ_TIME_INTERVAL 100 ////time interval between requests to the serial port in us

#define RAW_STARTUP_TIMEOUT 5.0//maximum time (s) to wait for first data to arrive from the LTA, raw readout
#define READ_STARTUP_TIMEOUT 5.0//maximum time (s) to wait for first data to arrive from the LTA, CDS readout

#define EXPOSURE_START_DELAY 5.0//how long we wait before adjusting voltages to their "exposure" values, in sec (this allows for clearing at the start of the sequencer)

#define LTASOFT_MULTI_WORD "multi"
#define LTASOFT_NOMULTI_WORD "nomulti"
#define LTASOFT_READ_WORD "read"
#define LTASOFT_TRANSFER_RAW_WORD "getraw"
#define LTASOFT_STOP_WORD "stop"
#define LTASOFT_READOFF_WORD "readoff"
#define LTASOFT_GET_WORD "getsoft"
#define LTASOFT_SET_WORD "setsoft"
#define LTASOFT_SEQ_WORD "seq"
#define LTASOFT_SMART_SEQ_WORD "sseq"
#define LTASOFT_RUN_SEQ_WORD "runseq"
#define LTASOFT_START_SEQ_WORD "startseq"
#define LTASOFT_GET_SEQ_WORD "getseq"
#define LTASOFT_CHANGE_OUTFILE_NAME_WORD "name"
#define LTASOFT_GET_OUTFILE_NAME_WORD "name?"
#define LTASOFT_GET_LAST_FILENAME_WORD "filename?"
#define LTASOFT_CHANGE_TEMPDIR_WORD "tempdir"
#define LTASOFT_EXTRA_WORD "extra"
#define LTASOFT_GET_COLS_WORD "cols?"
#define LTASOFT_CHANGE_COLS_WORD "cols"
#define LTASOFT_CALIBRATE_INTERLEAVING_WORD "calibrate"

using namespace std;

UserInterpreter::UserInterpreter(int listenfd, Ethernet * eth, udpSocket * dataSocket) {
    listenfd_ = listenfd;
    eth_ = eth;
    dump_ = new DumpToDisc(dataSocket);
    data_ = new DataConversion;

    readoutStartTime = new char[80];
    readoutEndTime = new char[80];
    readoutStartTime[0]='\0';
    readoutEndTime[0]='\0';
    writeTimestamps_ = 1;

    cols = 1024;
    rows = 512;



    outDir_ = "images";
    outBasename_ = "image_lta_";
    tempDir_ = "temp";
    imageInd = 0;
    readoutType = 0;
    isMulti = false;
}

UserInterpreter::~UserInterpreter() {
    delete[] readoutStartTime;
    delete[] readoutEndTime;
    delete data_;
    delete dump_;
}

void UserInterpreter::change_name_outfile() {
    // Output file name.
    stringstream outFileNameStream;
    stringstream tempFileNameStream;
    imageInd ++;
    if (isMulti) {
        outFileNameStream << outDir_ << "/" << outBasename_ << ltaName_ << "_" << imageInd;
        tempFileNameStream << tempDir_ << "/" << outBasename_ << ltaName_ << "_" << imageInd;
    } else {
        outFileNameStream << outDir_ << "/" << outBasename_ << imageInd;
        tempFileNameStream << tempDir_ << "/" << outBasename_ << imageInd;
    }
    outFileName = outFileNameStream.str();
    tempFileName = tempFileNameStream.str();
}

int UserInterpreter::listenForCommands()
{
    time_t ticks;
    char recvBuff[1025] = "";

    // Block until a new connection is pending.
    client_sock_ = accept(listenfd_, (struct sockaddr*)NULL, NULL);
    if (client_sock_==-1) {//connection failed
        if (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) { //timeout or "interrupted system call" (harmless)
            return 0;
        }
        LOG_F(WARNING, "TCP error on terminal port, no message received: %s", strerror(errno));
        return 1; //we don't do anything with this return value, but we could
    }

    int read_size = recv(client_sock_, recvBuff , 1025 , 0);

    ticks = time(NULL);
    if (writeTimestamps_) {
        //JT: decluttering the communication channels with the users
	// //This is a bit of a kludge
        //dprintf(client_sock_, "# %.24s\n", ctime(&ticks));
    }

    if (read_size > 0)
    {
        string line = recvBuff;
        trimString(line);//get rid of leading and trailing whitespace

        //separar recvBuff en palabras
        vector <string> words = tokenize(recvBuff);

        stringstream printstream;
        // Print words.
        for (uint i = 0; i < words.size(); ++i)
        {
            printstream << "palabra[" << i << "]=\"" << words[i] << "\" ";
        }
        LOG_F(INFO, "%s", printstream.str().c_str());

        if (words.size()<1) {
            LOG_F(WARNING, "empty command? no words found, will do nothing");
            close(client_sock_);
            return 0;
        }

        int status = 0;

        // Choose command
        // Stop LTA daemon.
        if (words[0].compare(LTASOFT_STOP_WORD) == 0) {
            status = cmd_stop_software();
            if (status !=0) userInterpreterErrorMessage(status);
            return -1;
        }

        // Stop LTA simulator and daemon.
        else if (words[0].compare(LTASOFT_STOPSIM_WORD) == 0) {
            status = cmd_stop_simulator();
            if (status !=0) userInterpreterErrorMessage(status);
            return -1;
        }

        // Read image.
        else if (words[0].compare(LTASOFT_READ_WORD) == 0) {
            status = cmd_start_standard_readout();
        }

        // Get raw data from board buffer.
        else if (words[0].compare(LTASOFT_TRANSFER_RAW_WORD) == 0) {
            status = cmd_start_raw_readout();
        }

        // Stop current read.
        else if (words[0].compare(LTASOFT_READOFF_WORD) == 0) {
            status = cmd_stop_readout();
        }

        // Get daemon variable.
        else if (words[0].compare(LTASOFT_GET_WORD) == 0 && words.size()==2) {
            status = cmd_getsoft(words[1]);
        }

        // Set daemon variable.
        else if (words[0].compare(LTASOFT_SET_WORD) == 0 && words.size()==3) {
            status = cmd_setsoft(words[1], words[2]);
        }

        // Load new sequencer.
        else if (words[0].compare(LTASOFT_SEQ_WORD) == 0 && words.size()==2) {
            status = cmd_load_seq(words[1]);
        }

        // Load new smart sequencer.
        else if (words[0].compare(LTASOFT_SMART_SEQ_WORD) == 0 && words.size()==2) {
            status = cmd_load_smart_seq(words[1]);
        }

        // Run sequencer without reading data.
        else if (words[0].compare(LTASOFT_RUN_SEQ_WORD) == 0) {
            status = cmd_run_seq();
        }

        // Run sequencer without reading data.
        else if (words[0].compare(LTASOFT_START_SEQ_WORD) == 0) {
            status = cmd_start_seq();
        }

        // Get value of sequencer variable.
        else if (words[0].compare(LTASOFT_GET_SEQ_WORD) == 0 && words.size()==2) {
            status = cmd_get_seq_var(words[1]);
        }

        // Change output file name.
        else if (words[0].compare(LTASOFT_CHANGE_OUTFILE_NAME_WORD) == 0 && words.size()==2) {
            status = cmd_change_output_file_name(words[1]);
        }

        // Get output file name.
        else if (words[0].compare(LTASOFT_GET_OUTFILE_NAME_WORD) == 0) {
            status = cmd_print_output_file_name();
        }

        // Get full filename of last file.
        else if (words[0].compare(LTASOFT_GET_LAST_FILENAME_WORD) == 0) {
            status = cmd_print_last_filename();
        }

        // Change temp directory name.
        else if (words[0].compare(LTASOFT_CHANGE_TEMPDIR_WORD) == 0 && words.size()==2) {
            status = cmd_change_temp_dir(words[1]);
        }

        // Enable multi-LTA readout.
        else if (words[0].compare(LTASOFT_MULTI_WORD) == 0 && words.size()==2) {
            status = cmd_multi(words[1]);
        }

        // Disable multi-LTA readout.
        else if (words[0].compare(LTASOFT_NOMULTI_WORD) == 0) {
            status = cmd_nomulti();
        }

        // Get number of columns.
        else if (words[0].compare(LTASOFT_GET_COLS_WORD) == 0) {
            status = cmd_get_cols();
        }

        // Change number of columns.
        else if (words[0].compare(LTASOFT_CHANGE_COLS_WORD) == 0 && words.size()==2) {
            status = cmd_set_cols_var(words[1]);
        }

        // Extra variables from sequencer.
        else if (words[0].compare(LTASOFT_EXTRA_WORD) == 0)
        {
            status = cmd_get_extra_words();
        }

        // Calibrate gains for interleaving
        else if (words[0].compare(LTASOFT_CALIBRATE_INTERLEAVING_WORD) == 0)
        {
            status = cmd_get_calibrate_interleaving();
        }

        // If it is a smart sequencer variable, change it.
        else if (sseq.isSequencerVar(words[0]) && words.size()==2) {
            status = cmd_set_sseq_var(words[0], words[1]);
        }

        // If it is a sequencer variable, change it.
        else if (seq.isSequencerVar(words[0]) && words.size()==2) {
            status = cmd_set_seq_var(words[0], words[1]);
        }

        else {
            //Command for board
            status = cmd_send_command_to_board(line);
        }
        if (status !=0) userInterpreterErrorMessage(status);
    }

    close(client_sock_);
    return 0;
}


void userInterpreterErrorMessage(const int status)
{
    LOG_F(WARNING, "error executing command in listeningForCommands!!!");
}

int UserInterpreter::talkToBoard(const string &sendBuff, const string &expResp, string &boardResponse, const double workFlowTimeOut)
{
    int status = 0;

    // JT: don't send it by default
    // //send user what is being sent to board
    //dprintf(client_sock_ , "%s\n", sendBuff.c_str());

    //send message to board
    status = eth_->sendCmdEthe(sendBuff);
    
    // JT: don't send it by default
    // //update user
    //dprintf(client_sock_ , "Command Sent\n");
    
    //listen to board
    status = eth_->listenForBoardResponse(expResp, boardResponse, workFlowTimeOut);
    //trimString(boardResponse);

    // JT: don't send it by default. Only "get" commands or error messages should be reported
    // //send board message to the user
    //dprintf(client_sock_ , "%s\n", boardResponse.c_str());
    return status;
}

int UserInterpreter::sendCmdToBoard(const string sendBuff)
{
    string boardResponse;
    int status = talkToBoard(sendBuff, LTA_GEN_DONE_MESSAGE, boardResponse, SP_WORK_FLOW_TIMEOUT);
    if (status==3) {
        LOG_F(WARNING, "non-empty sendCmdToBoard response: %s", boardResponse.c_str());
        dprintf(client_sock_, "non-empty response: %s\n", boardResponse.c_str());
    }
    return status;
}

int UserInterpreter::cmd_send_command_to_board(const string &line)
{
    int status = send_command_to_board(line);
    if (status==3) status = 0; //don't require an exact match
    close(client_sock_);

    return status;
}

int UserInterpreter::send_command_to_board(const string &line)
{
    string sendBuff = line;
    sendBuff += "\r";
    //talk to board
    string boardResponse;
    int status = talkToBoard(sendBuff, LTA_GEN_DONE_MESSAGE, boardResponse, SP_WORK_FLOW_TIMEOUT);

    //JT: only report "get" requests done through command line
    if (string("get").compare(line.substr(0, 3)) == 0){
       dprintf(client_sock_ , "%s\n", boardResponse.c_str());
    }
    return status;
}

string UserInterpreter::read_board_var(const string &var)
{
    string sendBuff = "get ";
    sendBuff.append(var);
    sendBuff += "\r";
    //talk to board
    string boardResponse;
    talkToBoard(sendBuff, LTA_GEN_DONE_MESSAGE, boardResponse, SP_WORK_FLOW_TIMEOUT);
    return boardResponse;
}

int UserInterpreter::cmd_stop_software()
{
    dprintf(client_sock_, "Stop: LTA daemon.\n");
    close(client_sock_);
    return 0;
}

int UserInterpreter::cmd_stop_simulator()
{
    string sendBuff = LTASOFT_STOPSIM_WORD;
    sendBuff += "\r";
    int status = sendCmdToBoard(sendBuff);

    dprintf(client_sock_, "Stop: LTA simulator.\n");

    close(client_sock_);
    return status;
}

int UserInterpreter::cmd_get_extra_words()
{				
    //get sequencer variables
    vector <string> varName;
    vector <string> varValue;
    vector <vector <int>> varUsePos;
    seq.getSeqVars(varName, varUsePos, varValue);

    for (uint i = 0; i < varName.size(); ++i)
    {
        //if (varUsePos[i].size()>0)
        {
            // Send message to terminal.
            dprintf(client_sock_, "%s = %s , positions in seq: ", varName[i].c_str(), varValue[i].c_str());

            for (uint j = 0; j < varUsePos[i].size(); ++j)
            {
                dprintf(client_sock_, "%d ", varUsePos[i][j]);
            }
            dprintf(client_sock_, "\n");
        }
    }

    //print calculated gains
    data_->print_interleaving();

    close(client_sock_);

    return 0;
}

int UserInterpreter::cmd_set_seq_var(const string varName, const string varValue)
{
    int status = set_seq_var(varName, varValue);

    //calculate the output data number of columns and rows if the sequencer has this variables
    update_seq_dimensions();

    close(client_sock_);
    return status;
}

int UserInterpreter::set_seq_var(const string varName, const string varValue)
{
    // Get variable value.
    string varValueStr = varValue;

    // Change variable value.
    int status = seq.changeSeqVarValue(varName, varValueStr);
    if (status != 0) {
        LOG_F(ERROR, "something went wrong trying to change sequencer variable value");
        return 1;
    }

    //now load the sequencer in the software
    status = seq.loadSequencer();
    if (status != 0)
    {
        LOG_F(ERROR, "something wrong happened loading the sequencer after changing variable value, status %d", status);
        close(client_sock_);
        return 1;
    }

    status = load_seq();

    return 0;
}

int UserInterpreter::cmd_get_seq_var(const string varName)
{
    // Print the value of a sequencer variable (either standard or smart)
    string varValue;
    int status = get_seq_var(varName, varValue);

    if (status == 0) { 
        dprintf(client_sock_, "%s = %s\n", varName.c_str(), varValue.c_str());
    }

    else {
        dprintf(client_sock_, "Invalid sequencer variable: %s\n", varName.c_str());
    }        

    close(client_sock_);

    // Always return 0 for get commands?
    return 0;
}

int UserInterpreter::get_seq_var(const string varName, string &varValue)
{
    // Get the value of a sequencer variable (either standard or smart)
    if (sseq.isSequencerVar(varName)) { 
        varValue = sseq.resolveVar(varName);
    }
    
    else if (seq.isSequencerVar(varName)) {
        seq.getSequencerVarValue(varName, varValue);
    }

    else {
      return 1;
    }

    return 0;
}

int UserInterpreter::update_seq_dimensions()
{
    //calculate the output data number of columns and rows if the sequencer has this variables
    if (!seq.isSequencerVar(SEQ_CONVENTION_READ_NSAMP_NUMBER) || !seq.isSequencerVar(SEQ_CONVENTION_READ_ROWS_NUMBER) || !seq.isSequencerVar(SEQ_CONVENTION_READ_COLS_NUMBER)){//one of the dimension variables is not defined
        LOG_F(WARNING, "no calculation of output image size");
        LOG_F(WARNING, "rows and cols number should be added by the user");
    } else {
        string NSAMP;
        string NCOLS;
        string NROWS;
        seq.getSequencerVarValue(SEQ_CONVENTION_READ_NSAMP_NUMBER,NSAMP);
        seq.getSequencerVarValue(SEQ_CONVENTION_READ_ROWS_NUMBER,NROWS);
        seq.getSequencerVarValue(SEQ_CONVENTION_READ_COLS_NUMBER,NCOLS);
        stringstream transNSAMP;
        transNSAMP << NSAMP;
        unsigned int nsamp;
        transNSAMP >> nsamp;
        stringstream transNCOLS;
        transNCOLS << NCOLS;
        unsigned int ncols;
        transNCOLS >> ncols;
        stringstream transNROWS;
        transNROWS << NROWS;
        unsigned int nrows;
        transNROWS >> nrows;
        cols = ncols*nsamp;
        rows = nrows;
    }
    return 0;
}

int UserInterpreter::cmd_load_seq(const string seqFileName)
{
    int status = seq.loadNewSequencer(seqFileName);
    if (status != 0)
    {
        LOG_F(WARNING, "sequencer is damaged, status %d", status);
        close(client_sock_);
        return 1;
    }

    status = load_seq();

    //check if sequencer request an interleaving readout
    if (seq.isSequencerVar(SEQ_CONVENTION_INTERLEAVING)) readoutType = LTASOFT_READOUT_TYPE_INTERLEAVING;
    else readoutType = LTASOFT_READOUT_TYPE_STANDARD;

    //calculate the output data number of columns and rows if the sequencer has this variables
    update_seq_dimensions();
    LOG_F(INFO, "VIVA LA PATRIA %d", cols);

    close(client_sock_);

    return 0;
}

int UserInterpreter::load_seq()
{
    int status = 0;

    // Stop any readout.
    // Stop sequencer.
    status = sendCmdToBoard("set seqStart 0\r");

    // Stop packer.
    //status = sendCmdToBoard("set packStart 0\r");

    // Clear sequencer.
    status = sendCmdToBoard("set seq clear\r");

    // Upload new sequencer to board.
    unsigned int compSeqNInst = seq.getNumberOfSeqInstruction();
    for (uint i = 0; i < compSeqNInst; ++i)
    {
        stringstream sendBuffStream;

        //change instruction in sequencer						// Command.
        string inst;
        seq.getInstruction(i,inst);
        sendBuffStream << "set seq " << i << " " << strtol(inst.c_str(), NULL, 2) << "\r";
        status = sendCmdToBoard(sendBuffStream.str());
    }

    // Make loaded sequencer active.
    status = sendCmdToBoard("set seq load\r");

    return status;
}

int UserInterpreter::cmd_set_sseq_var(const string varName, const string varValue)
{
    int status = 0;
    string currVal = sseq.resolveVar(varName);
    LOG_F(INFO, "var %s: current value %s, new value %s", varName.c_str(), currVal.c_str(), varValue.c_str());
    if (currVal.compare(varValue)!=0) {
        sseq.changeSeqVarValue(varName, varValue);

        status = load_smart_seq();
    }

    if (status != 0)
    {
        LOG_F(ERROR, "failed to load new sequencer, status %d", status);
        close(client_sock_);
        return 1;
    }

    close(client_sock_);
    return status;
}


int UserInterpreter::cmd_load_smart_seq(const string seqFileName)
{
    //load sequencer in ram
    try {
        sseq.loadNewSequencer(seqFileName);
    } catch (sequencerException& e) {
        LOG_F(WARNING, "sequencer exception: %s", e.what());
        close(client_sock_);
        return 1;
    }

    int status = load_smart_seq();
    if (status != 0)
    {
        LOG_F(WARNING, "failed to load new sequencer, status %d", status);
        close(client_sock_);
        return 1;
    }
    close(client_sock_); 
    return status;
}

int UserInterpreter::load_smart_seq()
{
    if (sseq.isSequencerVar(SEQ_CONVENTION_READ_ROWS_NUMBER)) {
        rows = sseq.resolveIntVar(SEQ_CONVENTION_READ_ROWS_NUMBER);
    }
    long ncols = -1;
    long nsamp = -1;
    if (sseq.isSequencerVar(SEQ_CONVENTION_READ_COLS_NUMBER)) {
        ncols = sseq.resolveIntVar(SEQ_CONVENTION_READ_COLS_NUMBER);
    }
    if (sseq.isSequencerVar(SEQ_CONVENTION_READ_NSAMP_NUMBER)) {
        nsamp = sseq.resolveIntVar(SEQ_CONVENTION_READ_NSAMP_NUMBER);
    }
    if (ncols!=-1 && nsamp!=-1) {
        cols = sseq.resolveIntVar(SEQ_CONVENTION_READ_COLS_NUMBER) * sseq.resolveIntVar(SEQ_CONVENTION_READ_NSAMP_NUMBER);
    }
    LOG_F(INFO,"new image dimensions: rows=%d, ncols=%ld, nsamp=%ld, cols=%d", rows, ncols, nsamp, cols);
    LOG_F(INFO,"new seq duration: %s", printTime(sseq.getDuration()).c_str());

    int status = 0;

    // Stop sequencer.
    status = sendCmdToBoard("sek reset\r");
    // Stop packer.
    //status = sendCmdToBoard("set packStart 0\r");

    //clear sequencer recipes
    status = sendCmdToBoard("sek clear recipes\r");
    //clear sequencer dynamic vector
    status = sendCmdToBoard("sek clear dynamicVector\r");
    // Upload new sequencer to board
    try {
        vector<string> commToSend = sseq.makeCommandsToBoard();
        for (uint i = 0; i < commToSend.size(); ++i)
        {
            LOG_F(1, "sending %s", commToSend[i].c_str());
            status = sendCmdToBoard(commToSend[i]);
            if (status!=0) return status;
        }
    } catch (sequencerException& e) {
        LOG_F(WARNING, "sequencer exception: %s", e.what());
        return 1;
    }

    return status;
}

int UserInterpreter::cmd_set_cols_var(const string word)
{
    // Check if argument is a number.
    if (word.find_first_not_of( "0123456789" ) != string::npos)
    {
        LOG_F(WARNING, "bad number of columns...nothing done");
    }
    else
    {
        // Get number of columns from argument.
        long colsTemp = atol(word.c_str());

        if (colsTemp <= 0)
        {
            LOG_F(WARNING, "bad number of columns...should be greater than 0...nohting done");
        }
        else
        {
            cols = colsTemp;

            // Send message to terminal.
            dprintf(client_sock_, "Number of columns in the output image: %d\n", cols);
        }
    }

    close(client_sock_);
    return 0;
}

int UserInterpreter::cmd_getsoft(const string varName)
{
    string varValue;
    if (varName.compare("writeTimestamps")==0) {
        varValue = get_write_timestamps();
    } else if (varName.compare("deleteDats")==0) {
        varValue = get_delete_dats();
    } else if (varName.compare("keepTemp")==0) {
        varValue = get_keep_temp();
    } else if (varName.compare("name")==0) {
        varValue = get_output_name();
    } else if (varName.compare("filename")==0) {
        varValue = get_output_filename();
    } else if (varName.compare("tempdir")==0) {
        varValue = get_temp_dir();
    } else if (varName.compare("multi")==0) {
        varValue = get_multi_lta();
    } else if (varName.compare("ltaName")==0) {
        varValue = get_lta_name();
    } else if (varName.compare("imageInd")==0) {
        varValue = get_image_index();
    }

    if (!varValue.empty()) {
        dprintf(client_sock_, "%s = %s\n", varName.c_str(), varValue.c_str());
    } else {
        dprintf(client_sock_, "Invalid daemon variable: %s\n", varName.c_str());
    }

    close(client_sock_);
    return 0;
}

int UserInterpreter::cmd_setsoft(const string varName, const string varValue)
{
    if (varName.compare("writeTimestamps")==0) {
        set_write_timestamps(varValue);
    } else if (varName.compare("deleteDats")==0) {
        set_delete_dats(varValue);
    } else if (varName.compare("keepTemp")==0) {
        set_keep_temp(varValue);
    } else if (varName.compare("name")==0) {
        set_output_name(varValue);
    } else if (varName.compare("filename")==0) {
        set_output_filename(varValue);
        dprintf(client_sock_, "Cannot change filename directly.");
    } else if (varName.compare("tempdir")==0) {
        set_temp_dir(varValue);
    } else if (varName.compare("multi")==0) {
        set_multi_lta(varValue);
    } else if (varName.compare("ltaName")==0) {
        set_lta_name(varValue);
    } else if (varName.compare("imageInd")==0) {
        set_image_index(varValue);
    } else {
        dprintf(client_sock_, "Invalid daemon variable: %s\n", varName.c_str());
    }

    close(client_sock_);
    return 0;
}

int UserInterpreter::cmd_get_cols()
{
    // Send message to terminal.
    dprintf(client_sock_, "Number of columns in the output image: %d\n", cols);

    close(client_sock_);
    return 0;
}


int UserInterpreter::cmd_print_output_file_name()
{
    // Print output file name.
    // Send message to terminal.
    dprintf(client_sock_, "%s/%s\n", outDir_.c_str(), outBasename_.c_str());

    close(client_sock_);
    return 0;
}

int UserInterpreter::cmd_print_last_filename()
{
    // Print output file name.
    // Send message to terminal.
    dprintf(client_sock_, "%s\n", data_->get_last_filename().c_str());

    close(client_sock_);
    return 0;
}


int UserInterpreter::cmd_change_output_file_name(const string fileName)
{
    set_output_name(fileName);

    // Send message to terminal.
    dprintf(client_sock_, "new file path: %s/%s\n", outDir_.c_str(), outBasename_.c_str());

    close(client_sock_);

    return 0;
}

int UserInterpreter::cmd_change_temp_dir(const string tempDir)
{
    set_temp_dir(tempDir);

    // Send message to terminal.
    dprintf(client_sock_, "new temp dir: %s\n", tempDir_.c_str());

    close(client_sock_);

    return 0;
}

int UserInterpreter::cmd_multi(const string ltaName)
{
    //Enable multi-LTA mode, and set the LTA name.
    set_multi(true);
    set_lta_name(ltaName);

    // Send message to terminal.
    dprintf(client_sock_, "enable multi-LTA, LTA name: %s\n", ltaName.c_str());

    close(client_sock_);

    return 0;
}

int UserInterpreter::cmd_nomulti()
{
    //Disable multi-LTA mode.
    set_multi(false);

    // Send message to terminal.
    dprintf(client_sock_, "disable multi-LTA\n");

    close(client_sock_);

    return 0;
}

int UserInterpreter::cmd_stop_readout()
{
    int status = 0;

    dump_->stop_readout_thread();

    // Stop packer.
    status = sendCmdToBoard("set packStart 0\r");

    if (seq.getSeqGood()) {//legacy sequencer
        // Stop sequencer.
        status = sendCmdToBoard("set seqStart 0\r");
    } else {//smart sequencer
        // Stop sequencer.
        status = sendCmdToBoard("sek reset\r");
    }

    if (isMulti) { //reset syncStop
        status = sendCmdToBoard("set syncStop 0\r");
    }

    close(client_sock_);
    return status;
}

int UserInterpreter::cmd_start_raw_readout()
{
    change_name_outfile();
    int status = 0;
    long sampsExpected = SMART_BUFFER_SIZE;
    status = dump_->start_readout_thread(client_sock_, outFileName, &inFileName, sampsExpected);
    // Check if read process has already started.
    if (status == 0)
    {
        if (seq.getSeqGood()) {//legacy sequencer
            // Start packer.
            status = sendCmdToBoard("set bufTraStart 1\r");
        } else {//smart sequencer
            // Start packer.
            status = sendCmdToBoard("set bufStart 1\r");
        }

        int timeref;
        fits_get_system_time(readoutStartTime, &timeref, &status);

        timespec timeoutTime;
        get_end_time(&timeoutTime, RAW_STARTUP_TIMEOUT);

        // wait for readout thread to report that no more data is arriving
        while (!dump_->is_readout_done()) {
            if (time_is_up(&timeoutTime) && !dump_->is_readout_getting_data()) {
                LOG_F(WARNING, "We never got data from the LTA");
                dprintf(client_sock_, "We never got data from the LTA\n");
                break;
            }
            usleep(1000);
        }

        cleanup_readout(LTA_GEN_RAW_TRANSFER_DONE_MESSAGE);

        // Convert image to RAW.
        data_->setInFileNames(&inFileName);
        status = data_->bin_to_raw(outFileName);
        if (status != 0)
        {
            LOG_F(WARNING, "something went wrong trying to translate output data");
        }
        LOG_F(INFO, "raw data translated to human readable format");
        LOG_F(INFO, "transfer done");
    }

    close(client_sock_);

    return 0;
}

int UserInterpreter::cmd_start_standard_readout()
{
    long sampsExpected;
    switch (readoutType) {
        case LTASOFT_READOUT_TYPE_STANDARD:
            if (sseq.getSeqGood() && sseq.dynVarInUse()) {//smart sequencer, with dynamic variable
                sampsExpected = -1;
            } else {//legacy sequencer, or non-dynamic smart sequencer
                sampsExpected = cols*rows*LTA_NUMBER_OF_CHANNELS;
            }
            break;
        case LTASOFT_READOUT_TYPE_INTERLEAVING:
            sampsExpected = cols*rows*LTA_NUMBER_OF_CHANNELS;
            break;
        default:
            sampsExpected = -1;
            break;
    }
    bool dynamicReadout = sseq.getSeqGood() && sseq.dynVarInUse(); //smart sequencer, with dynamic variable
    int status = run_readout(sampsExpected);
    if (status ==0) {

        //request vars to put in the fits image
        string strVars = "";
        stringstream strVarsStream;
        //get firmware variables
        string sendBuff;
        string boardResponse;
        sendBuff.clear();
        boardResponse.clear();
        sendBuff = "get all\r";
        status = talkToBoard(sendBuff, LTA_GEN_DONE_MESSAGE, boardResponse, SP_WORK_FLOW_TIMEOUT);
        strVarsStream << boardResponse;
        //get telemetry variables
        sendBuff.clear();
        boardResponse.clear();
        sendBuff = "get telemetry all\r";
        status = talkToBoard(sendBuff, LTA_GEN_DONE_MESSAGE, boardResponse, SP_WORK_FLOW_TIMEOUT);
        strVarsStream << boardResponse;
        strVars = strVarsStream.str();

        vector<imageVar> vars = make_var_list(strVars, (!dynamicReadout && readoutType==LTASOFT_READOUT_TYPE_STANDARD), true);
        
        //Add the sequencer as a variable if using smart sequencer
        if (sseq.getSeqGood()){
            imageVar seqVar;
            seqVar.name  = "SEQ";
            seqVar.value = sseq.getSequencerContent(true, true);
            vars.push_back(seqVar);
        }

        data_->setInFileNames(&inFileName);

        //convert file to fits
        switch (readoutType) {
            case LTASOFT_READOUT_TYPE_STANDARD:
                LOG_F(INFO, "making conversion from standard readout ");
                LOG_F(INFO, "va a entrar a la nueva funcion ");
                if (dynamicReadout) {//smart sequencer, with dynamic variable
                    //get output coordinates
                    smartImageDimensions_t dims = sseq.calculateSmartImageDimensions();

                    status = data_->bin_to_fits_smart(outFileName, dims, vars);
                } else {//legacy sequencer, or non-dynamic smart sequencer
                    status = data_->bin_to_fits(outFileName, tempFileName, cols, rows, imageInd, vars);
                }
                break;
            case LTASOFT_READOUT_TYPE_INTERLEAVING:
                LOG_F(INFO, "MAKING CONVERSION FROM INTERLEAVING ");
                status = data_->bin_to_fits_interleaving(outFileName, cols, rows, vars);
                break;
            default:
                break;
        }
        if (status != 0)
        {
            LOG_F(WARNING, "something went wrong trying to translate output data");
        }

        // Send message to terminal.
        dprintf(client_sock_, "Image translated to fits\n");
    }
    close(client_sock_);
    return 0;
}

int UserInterpreter::run_readout(const long sampsExpected)
{
    change_name_outfile();
    int status = 0;

    // Start packer.
    status = sendCmdToBoard("set packStart 1\r");

    string datFileName; //path (minus suffix) for .dat files
    if (readoutType==LTASOFT_READOUT_TYPE_STANDARD && (!sseq.getSeqGood() || !sseq.dynVarInUse())) {
        //standard readout with non-dynamic sequencer - dat files go in temp directory (they'll get deleted if they pass checks)
        datFileName = tempFileName;
    } else {
        //something else (interleaving? dynamic?)
        datFileName = outFileName;
    }
    status = dump_->start_readout_thread(client_sock_, datFileName, &inFileName, sampsExpected);

    bool doExposure = false;
    bool doVoltageShift = false;
    bool toggleVDD = false;
    bool toggleVR = false;
    bool toggleVDR = false;
    double exposure_seconds = 0.0;
    map<string,float> readoutVoltages;
    map<string,float> exposeVoltages;

    if (seq.getSeqGood()) {//legacy sequencer
        //TODO: if we ever want to do this with the legacy sequencer?
    } else {//smart sequencer
        if (sseq.isSequencerVar(SEQ_CONVENTION_EXPOSURE_SECONDS)) {
            doExposure = true;
            exposure_seconds = sseq.resolveIntVar(SEQ_CONVENTION_EXPOSURE_SECONDS);
            if (sseq.isSequencerVar(SEQ_CONVENTION_TOGGLE_VDD) && sseq.resolveIntVar(SEQ_CONVENTION_TOGGLE_VDD)!=0) {
                toggleVDD = true;
            }
            if (sseq.isSequencerVar(SEQ_CONVENTION_TOGGLE_VR) && sseq.resolveIntVar(SEQ_CONVENTION_TOGGLE_VR)!=0) {
                toggleVR = true;
            }
            if (sseq.isSequencerVar(SEQ_CONVENTION_TOGGLE_VDR) && sseq.resolveIntVar(SEQ_CONVENTION_TOGGLE_VDR)!=0) {
                toggleVDR = true;
            }

            if (exposure_seconds > EXPOSURE_START_DELAY) {
                doVoltageShift = true;
                //figure out the voltages we need to change during exposure
                vector<string> expVars = sseq.getPrefixedVars("EXP_");
                for(std::vector<string>::iterator it = expVars.begin(); it != expVars.end(); ++it) {
                    float expAdjustment = sseq.resolveFloatVar(*it);
                    string vName = it->substr(4,string::npos);
                    string response = read_board_var(vName);
                    vector<string> responseVars;
                    vector<string> responseVals;
                    readInternalVars(response, responseVars, responseVals);
                    if (responseVars.size()==1 && vName.compare(responseVars[0])==0) {
                        float currentVoltage = atof(responseVals[0].c_str());
                        readoutVoltages[vName] = currentVoltage;
                        exposeVoltages[vName] = currentVoltage+expAdjustment;
                        LOG_F(INFO, "Will use %s = %f during exposure, %f during readout", vName.c_str(), exposeVoltages[vName], readoutVoltages[vName]);
                    } else {
                        LOG_F(WARNING, "Failed to read voltage %s", vName.c_str());
                    }
                }
                //printf("%d\n",readoutVoltages.size());
            }
        }

    }

    if (status==0) {
        if (doExposure) {
            if (toggleVDD) {
                LOG_F(INFO, "start of exposure, disabling VDD");
                send_command_to_board("set vdd_sw 0");
            }
            if (toggleVR) {
                LOG_F(INFO, "start of exposure, disabling VR");
                send_command_to_board("set vr_sw 0");
            }
            if (toggleVDR) {
                LOG_F(INFO, "start of exposure, disabling VDR");
                send_command_to_board("set vdrain_sw 0");
            }
        }

        //start the sequencer
        run_seq();
        LOG_F(INFO,"first data expected in: %s", printTime(sseq.getTimeUntilData()+exposure_seconds).c_str());

        int timeref;
        fits_get_system_time(readoutStartTime, &timeref, &status);

        const double time_until_data = sseq.getTimeUntilData();
        double startup_timeout = READ_STARTUP_TIMEOUT + exposure_seconds + time_until_data;

        bool doneClearing = false;
        timespec exposureStartTime;
        get_end_time(&exposureStartTime, EXPOSURE_START_DELAY);

        bool doneExposing = false;
        timespec exposureEndTime;
        get_end_time(&exposureEndTime, exposure_seconds);

        timespec timeoutTime;
        get_end_time(&timeoutTime, startup_timeout);

        // wait for readout thread to report that no more data is arriving
        while (!dump_->is_readout_done()) {
            if (time_is_up(&timeoutTime) && !dump_->is_readout_getting_data()) {
                LOG_F(WARNING, "We never got data from the LTA");
                dprintf(client_sock_, "We never got data from the LTA\n");
                break;
            }
            if (doVoltageShift && !doneClearing && time_is_up(&exposureStartTime)) {
                char toSend[100];
                for(std::map<string,float>::iterator it = exposeVoltages.begin(); it != exposeVoltages.end(); ++it) {
                    snprintf(toSend, 100, "set %s %.3f", it->first.c_str(), it->second);
                    send_command_to_board(toSend);
                    LOG_F(INFO, "start of exposure, set %s %.3f", it->first.c_str(), it->second);
                }
                doneClearing = true;
            }
            if (doExposure && !doneExposing && time_is_up(&exposureEndTime)) {
                if (toggleVDD) {
                    LOG_F(INFO, "end of exposure, enabling VDD");
                    send_command_to_board("set vdd_sw 1");
                }
                if (toggleVR) {
                    LOG_F(INFO, "end of exposure, disabling VR");
                    send_command_to_board("set vr_sw 1");
                }
                if (toggleVDR) {
                    LOG_F(INFO, "end of exposure, disabling VDR");
                    send_command_to_board("set vdrain_sw 1");
                }
                if (doVoltageShift) {
                    char toSend[100];
                    for(std::map<string,float>::iterator it = readoutVoltages.begin(); it != readoutVoltages.end(); ++it) {
                        snprintf(toSend, 100, "set %s %.3f", it->first.c_str(), it->second);
                        send_command_to_board(toSend);
                        LOG_F(INFO, "end of exposure, set %s %.3f", it->first.c_str(), it->second);
                    }
                }
                doneExposing = true;
            }
            usleep(1000);
        }

        fits_get_system_time(readoutEndTime, &timeref, &status);

        cleanup_readout(LTA_GEN_READ_DONE_MESSAGE);
    }

    return 0;
}

int UserInterpreter::cleanup_readout(const char *doneMessage)
{
    LOG_F(INFO, "readout thread has timed out, now we check if the LTA is done");

    // Wait until readout finishes.
    string boardMessage;
    boardMessage.clear();
    std::string doneStr = doneMessage;
    int status = eth_->listenForBoardResponse(doneStr, boardMessage, 10);

    trimString(doneStr);
    trimString(boardMessage);
    switch (status) {
        case 0:
            LOG_F(INFO, "got expected \"%s\"", boardMessage.c_str());
            break;
        default:
            LOG_F(WARNING, "did not get expected \"%s\" message from the LTA, instead got: %s", doneStr.c_str(), boardMessage.c_str());
            if (boardMessage.length()>0) {
                LOG_F(WARNING, "unexpected LTA response: %s", boardMessage.c_str());
            }
            break;
    }

    //send response to user
    dprintf(client_sock_, "%s\n", boardMessage.c_str());

    //wait some time in order to wait the soft to take all data from the socket, we need to do this better
    usleep(LTASOFT_DELAY_BEFORE_USER_CANCELLING_READOUT);

    return dump_->stop_readout_thread();
}

int UserInterpreter::cmd_run_seq()
{
    int status = 0;

    if (!dump_->is_readout_active()) {
        string sendBuff;
        string boardResponse;
        //start the sequencer
        run_seq();
        // Wait until readout finishes.
        string boardMessage;
        boardMessage.clear();
        status = eth_->listenForBoardResponse(LTA_GEN_READ_DONE_MESSAGE, boardMessage, SP_WORK_FLOW_TIMEOUT);

        //send response to user
        dprintf(client_sock_, "%s\n", boardMessage.c_str());
        LOG_F(INFO, "sequencer done");
    } else {
        dprintf(client_sock_,"currently reading or transfering data...nothing done\n");
    }
    close(client_sock_);
    return status;
}

int UserInterpreter::cmd_start_seq()
{
    int status = 0;

    if (!dump_->is_readout_active()) {
        //start the sequencer
        run_seq();

        //send response to user
        dprintf(client_sock_, "sequencer started\n");
        LOG_F(INFO, "sequencer started");
    } else {
        dprintf(client_sock_,"currently reading or transfering data...nothing done\n");
    }
    close(client_sock_);
    return status;
}

int UserInterpreter::run_seq()
{
    int status = 0;
    if (seq.getSeqGood()) {//legacy sequencer
        // Start sequencer.
        if (isMulti) {
            // Set source to Sync Stop block.
            status = sendCmdToBoard("set seqStartSrc 1\r");

            // Start sequencer.
            status = sendCmdToBoard("set syncStop 1\r");
        } else {
            status = sendCmdToBoard("set seqStart 1\r");
        }
    } else {//smart sequencer
        
        // Report seq duration
	time_t ticks = time(NULL);
        LOG_F(INFO,"seq duration: %s", printTime(sseq.getDuration()).c_str());
        dprintf(client_sock_, "seq started:  %.24s\n", ctime(&ticks));
	dprintf(client_sock_, "seq duration: %s\n", printTime(sseq.getDuration()).c_str());

        // Stop sequencer.
        status = sendCmdToBoard("sek reset\r");

        // Preload sequencer.
        status = sendCmdToBoard("sek preload\r");

        // Wait for sequencer to preload.
        usleep(100000);

        if (isMulti) {
            // Set source to Sync Stop block.
            status = sendCmdToBoard("sek sync_rdy\r");

            // Start sequencer.
            status = sendCmdToBoard("set syncStop 1\r");
        } else {
            // Start sequencer.
            status = sendCmdToBoard("sek start\r");
        }
    }
    return status;
}

vector<imageVar> UserInterpreter::make_var_list(const string strVars, bool hduVars, bool getInternalVars)
{
    vector<imageVar> vars;
    // TODO: check for duplicate names?

    //get sequencer variables
    if (sseq.getSeqGood())
    {
        map<string,string> smartVars = sseq.getVars();
        for (map<string,string>::iterator it=smartVars.begin(); it!=smartVars.end(); ++it) {
            imageVar var = {it->first,it->second,"Smart sequencer variable"};
            vars.push_back(var);
        }
    }
    else
    {
        vector <string> varName;
        vector <string> varValue;
        vector <vector <int>> varUsePos;
        seq.getSeqVars(varName, varUsePos, varValue);
        for (unsigned int i = 0; i < varName.size(); ++i)
        {
            imageVar var = {varName[i],varValue[i],"Sequencer variable"};
            vars.push_back(var);
        }
    }

    if (getInternalVars) {
        //get LTA internal variables
        vector<string> interVarsNames;
        vector<string> interVarsVals;
        int headerVarStatus = readInternalVars(strVars, interVarsNames, interVarsVals);
        for (uint i = 0; i < interVarsNames.size(); ++i){
            transform(interVarsNames[i].begin(), interVarsNames[i].end(), interVarsNames[i].begin(), ::toupper);
            if(interVarsNames[i] == "END") interVarsNames[i]="END_"; // FITS format can't have a variable named END in the header
        }
        // copy vector
        interVarsNamesPrev = interVarsNames;

        /* Write the LTA internal variables in the FITS header */
        if (headerVarStatus==0){
            for (uint i = 0; i < interVarsNames.size(); ++i){
                imageVar var = {interVarsNames[i],interVarsVals[i],"Internal variable"};
                vars.push_back(var);
            }
        }
    }
    else for (uint i = 0; i < interVarsNamesPrev.size(); ++i){
            vars.push_back({interVarsNamesPrev[i],"",""});
        }

    if (isMulti) {
        imageVar var = {"ISMULTI","1","Multi-LTA mode"};
        vars.push_back(var);
        var = {"LTANAME",ltaName_.c_str(),"Identifier for this LTA in the multi-LTA system"};
        vars.push_back(var);
    } else {
        imageVar var = {"ISMULTI","0","Multi-LTA mode"};
        vars.push_back(var);
    }

    if (writeTimestamps_) {
        imageVar var = {"DATESTART",readoutStartTime,"Timestamp at start of readout"};
        vars.push_back(var);
        var = {"DATEEND",readoutEndTime,"Timestamp at end of readout"};
        vars.push_back(var);
    }
    if (hduVars) {
        vars.push_back({"chID","",""});
        vars.push_back({"RUNID","",""});
        vars.push_back({"NPIX","",""});
    }
    return vars;
}

int UserInterpreter::write_internal_vars()
{
    vector<imageVar> vars;
    return 0;
}

int UserInterpreter::readInternalVars(const string &strVars, vector<string> &interVarsNames, vector<string> &interVarsVals){

    istringstream issVars(strVars);
    while(!issVars.eof()) {
        string line;
        getline(issVars, line);
        if(line=="") continue;
        stringstream printstream;

        std::replace( line.begin(), line.end(), '=', ' ');
        istringstream issLine(line);
        string aux;
        std::vector<string> vAux;
        while (issLine >> aux){
            vAux.push_back(aux);
            printstream << aux << " ";
        }
        if(vAux.size()==2){
            printstream << "  OK";
            interVarsNames.push_back(vAux[0]);
            interVarsVals.push_back(vAux[1]);
        }
        else if (vAux.size()==0) {
            //do nothing, this is a blank line
        }
        else if (vAux.size()==1 && vAux[0].compare(0,4,LTA_GEN_DONE_MESSAGE,0,4)==0) {
            //do nothing, this is the "Done" at the end of a response
        }
        else if (vAux.size()==3 && vAux[2].compare("kHz")==0) {
            //clock frequency variable
            printstream << "  OK";
            interVarsNames.push_back(vAux[0]);
            interVarsVals.push_back(vAux[1]);
        }
        else if (vAux.size()>2 && vAux[0].compare("###")==0 && vAux[vAux.size()-1].compare("###")==0) {
            //do nothing, this is a header of the form "### Blah Blah ###"
        }
        else{
            printstream << "  Ignored";
            LOG_F(WARNING, "Error reading internal variables.");
            LOG_F(WARNING, "Format is NOT two columns");
            LOG_F(WARNING, "size %lu", vAux.size());
            LOG_F(WARNING, "Entry: %lu", interVarsNames.size());
        }
        LOG_F(1, "%s", printstream.str().c_str());
    }

    return 0;
}

int UserInterpreter::cmd_get_calibrate_interleaving()
{


    //check if the necessary variables are in the sequencer, if so change then to the desired value
    int varInd = 0;
    if (seq.isSequencerVar(SEQ_CONVENTION_CCD_COLS_NUMBER,varInd)!=0 || seq.isSequencerVar(SEQ_CONVENTION_CCD_ROWS_NUMBER,varInd)!=0 || seq.isSequencerVar(SEQ_CONVENTION_READ_NSAMP_NUMBER,varInd)!=0 || seq.isSequencerVar(SEQ_CONVENTION_READ_ROWS_NUMBER,varInd)!=0 || seq.isSequencerVar(SEQ_CONVENTION_READ_COLS_NUMBER,varInd)!=0 || seq.isSequencerVar(SEQ_CONVENTION_CCD_PRESCAN_NUMBER,varInd)!=0){
        LOG_F(WARNING, "some of the following sequencer variable is not define in the sequencer: %s %s %s %s %s %s", SEQ_CONVENTION_CCD_COLS_NUMBER, SEQ_CONVENTION_CCD_ROWS_NUMBER, SEQ_CONVENTION_READ_NSAMP_NUMBER, SEQ_CONVENTION_READ_ROWS_NUMBER, SEQ_CONVENTION_READ_COLS_NUMBER, SEQ_CONVENTION_CCD_PRESCAN_NUMBER);
        LOG_F(WARNING, "no calibration performed");

        //reponse to user
        dprintf(client_sock_,"No calibration performed. Some sequencer variables missing.\n");
        close(client_sock_);
        return 1;
    }
    //get information from the current sequencer
    string prevNSAMP;
    string prevNCOLS;
    string prevNROWS;
    seq.getSequencerVarValue(SEQ_CONVENTION_READ_NSAMP_NUMBER,prevNSAMP);
    seq.getSequencerVarValue(SEQ_CONVENTION_READ_ROWS_NUMBER,prevNROWS);
    seq.getSequencerVarValue(SEQ_CONVENTION_READ_COLS_NUMBER,prevNCOLS);
    //get CCD array information
    string CCDNCOL;
    string CCDNROW;
    seq.getSequencerVarValue(SEQ_CONVENTION_CCD_COLS_NUMBER, CCDNCOL);
    seq.getSequencerVarValue(SEQ_CONVENTION_CCD_ROWS_NUMBER,CCDNROW);

    LOG_F(INFO, "CCDNROW %s", CCDNROW.c_str());

    //find region limits and image size for new calibration
    stringstream transCCDNCOL;
    transCCDNCOL << CCDNCOL;
    unsigned int ccdncol;
    transCCDNCOL >> ccdncol;
    stringstream transCCDNROW;
    transCCDNROW << CCDNROW;
    unsigned int ccdnrow;
    transCCDNROW >> ccdnrow;
    unsigned int regMinCol = ccdncol + LTASOFT_CALIBRATION_NUMBER_PIXELS_SAFE_GOOD_REGION;
    unsigned int regMaxCol = regMinCol + LTASOFT_CALIBRATION_NUMBER_COLS;
    unsigned int newImageColNumber = regMaxCol + LTASOFT_CALIBRATION_NUMBER_PIXELS_SAFE_GOOD_REGION;
    unsigned int regMinRow = ccdnrow + LTASOFT_CALIBRATION_NUMBER_PIXELS_SAFE_GOOD_REGION;
    unsigned int regMaxRow = regMinRow + LTASOFT_CALIBRATION_NUMBER_ROWS;
    unsigned int newImageRowNumber = regMaxRow + LTASOFT_CALIBRATION_NUMBER_PIXELS_SAFE_GOOD_REGION;

    LOG_F(INFO, "newImageRowNumber %d regMaxRow %d regMinRow %d", newImageRowNumber, regMaxRow, regMinRow);
    //change sequencer variables for the calibration
    stringstream streamNSAMP;
    streamNSAMP << LTASOFT_CALIBRATION_NUMBER_NSAMP;
    int status = set_seq_var(SEQ_CONVENTION_READ_NSAMP_NUMBER, streamNSAMP.str());
    stringstream streamROWS;
    streamROWS << newImageRowNumber;
    status = set_seq_var(SEQ_CONVENTION_READ_ROWS_NUMBER, streamROWS.str());
    stringstream streamNCOLS;
    streamNCOLS << newImageColNumber;
    status = set_seq_var(SEQ_CONVENTION_READ_COLS_NUMBER, streamNCOLS.str());



    //get and set size for the output image file
    LOG_F(INFO, "size image for calibration %d rows %d cols", newImageRowNumber, newImageColNumber);
    //save original dimensions
    long prevCols = cols;
    long prevRows = rows;
    //set new dimensions
    cols = newImageColNumber;
    rows = newImageRowNumber;

    //read ccd
    long sampsExpected = cols*rows*LTA_NUMBER_OF_CHANNELS;
    status = run_readout(sampsExpected);

    string varValue;
    seq.getSequencerVarValue("NROW", varValue);
    LOG_F(INFO, "row number before caliberate %s", varValue.c_str());

    //make calibration
    vector<imageVar> vars = make_var_list("", false, true);
    data_->setInFileNames(&inFileName);

    status = data_->bin_to_calibrate(outFileName, regMinCol, regMaxCol, regMinRow, regMaxRow, cols, vars);

    //put back variables of the sequencer
    status = set_seq_var(SEQ_CONVENTION_READ_NSAMP_NUMBER, prevNSAMP);
    status = set_seq_var(SEQ_CONVENTION_READ_ROWS_NUMBER, prevNROWS);
    status = set_seq_var(SEQ_CONVENTION_READ_COLS_NUMBER, prevNCOLS);

    //put back original image size
    cols = prevCols;
    rows = prevRows;

    //reponse to user
    dprintf(client_sock_, "Calibration DONE.\n");
    close(client_sock_);
    return status;
}

// daemon variable setters

int UserInterpreter::set_write_timestamps(const string varValue)
{
    writeTimestamps_ = string_to_bool(varValue);
    data_->setWriteTimestamps(writeTimestamps_);
    return 0;
}

int UserInterpreter::set_output_name(const string varValue)
{
    char *dummy1 = strdup(varValue.c_str());
    char *dummy2 = strdup(varValue.c_str());

    // Change the name of the output data file.
    outDir_ = dirname(dummy1);
    outBasename_ = basename(dummy2);

    free(dummy1);
    free(dummy2);

    return 0;
}

int UserInterpreter::set_output_filename(const string varValue)
{
    LOG_F(WARNING, "Cannot set output filename explicitly.");
    return 1;
}
