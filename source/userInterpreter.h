#ifndef _userInterpreter_h_
#define _userInterpreter_h_

#include <string.h>

#include "udpSocket.h"
#include "ethernet.h"
#include "dumpToDisc.h"
#include "dataConversion.h"
#include "sequencerHandler.h"
#include "smartSequencerHandler.h"
#include "helperFunctions.h"

#define LTA_GEN_DONE_MESSAGE "Done\r\n"
#define LTA_GEN_READ_DONE_MESSAGE "Read done\r\n"
#define LTA_GEN_RAW_TRANSFER_DONE_MESSAGE "Transfer done\r\n"

#define LTASOFT_STOPSIM_WORD "stopsim"

using namespace std;

class UserInterpreter
{
    public:
        UserInterpreter(int listenfd, Ethernet * eth, udpSocket * dataSocket);
        ~UserInterpreter();
        int listenForCommands();
    private:
        int listenfd_; //persistent file descriptor for the socket
        int client_sock_; //socket for the current connection
        Ethernet * eth_;
        DumpToDisc * dump_;
        DataConversion * data_;
        //sequencer objects
        sequencerHandler seq;
        smartSequencerHandler sseq;
        char * readoutStartTime;
        char * readoutEndTime;
        int writeTimestamps_;

        uint cols;
        uint rows;

        string outDir_;
        string outBasename_;
        string tempDir_; //write files we expect to delete to a different directory

        std::vector <std::string> inFileName;
        string outFileName; //path (minus suffix) for output files
        string tempFileName; //path (minus suffix) for temp files
        int imageInd;
        int readoutType;
        bool isMulti;
        string ltaName_;

        vector<string> interVarsNamesPrev;

        void change_name_outfile();

        int talkToBoard(const string &sendBuff, const string &expResp, string &boardResponse, const double workFlowTimeOut);
        int sendCmdToBoard(const string sendBuff);//default timeout, wait for "Done" and warn on any other output

        int cmd_get_extra_words();
        int cmd_set_cols_var(const string word);
        int cmd_send_command_to_board(const string &line);
        int send_command_to_board(const string &line);
        string read_board_var(const string &var);
        int cmd_set_seq_var(const string varName, const string varValue);
        int set_seq_var(const string varName, const string varValue);
        int cmd_get_seq_var(const string varName);
        int get_seq_var(const string varName, string &varValue);
        int update_seq_dimensions();
        int cmd_set_sseq_var(const string varName, const string varValue);
        int cmd_get_sseq_var(const string varName);
        int cmd_getsoft(const string varName);
        int cmd_setsoft(const string varName, const string varValue);
        int cmd_get_cols();
        int cmd_print_output_file_name();
        int cmd_change_output_file_name(const string fileName);
        int cmd_print_last_filename();
        int cmd_change_temp_dir(const string tempDir);
        int cmd_multi(const string ltaName);
        int cmd_nomulti();
        int cmd_load_seq(const string seqFileName);
        int load_seq();
        int cmd_stop_readout();
        int cmd_start_raw_readout();
        int cmd_start_standard_readout();
        int cmd_stop_software();
        int cmd_stop_simulator();
        int run_readout(const long sampsExpected);
        int cleanup_readout(const char *doneMessage);
        int cmd_load_smart_seq(const string seqFileName);
        int load_smart_seq();
        int cmd_run_seq();
        int cmd_start_seq();
        int run_seq();
        vector<imageVar> make_var_list(const string strVars, bool hduVars, bool getInternalVars);
        int write_internal_vars();
        int readInternalVars(const string &strVars, vector<string> &interVarsNames, vector<string> &interVarsVals);
        int cmd_get_calibrate_interleaving();
        
        // LTA daemon software variable getters
        string get_write_timestamps() {return bool_to_string(data_->getWriteTimestamps());};
        string get_delete_dats() {return bool_to_string(data_->getDeleteDats());};
        string get_keep_temp() {return bool_to_string(data_->getKeepTemp());};
        string get_output_name() {return outDir_ + "/" + outBasename_;};
        string get_output_filename() {return data_->get_last_filename();};
        string get_temp_dir() {return tempDir_;};
        string get_multi_lta() {return bool_to_string(isMulti);};
        string get_lta_name() {return ltaName_;};
        string get_image_index() {return to_string(imageInd);};
        
        // LTA daemon software variable setters
        int set_write_timestamps(const string varValue);
        int set_delete_dats(const string varValue) {data_->setDeleteDats(string_to_bool(varValue)); return 0;};
        int set_keep_temp(const string varValue) {data_->setKeepTemp(string_to_bool(varValue)); return 0;};
        int set_output_name(const string varValue);
        int set_output_filename(const string varValue);
        int set_temp_dir(const string varValue) {tempDir_ = varValue; return 0;};
        int set_multi(const bool varValue) {isMulti = varValue; return 0;};
        int set_multi_lta(const string varValue) {set_multi(string_to_bool(varValue)); return 0;};
        int set_lta_name(const string varValue) {ltaName_ = varValue; return 0;};
        int set_image_index(const string varValue) {imageInd = atoi(varValue.c_str()); return 0;};

};

void userInterpreterErrorMessage(const int status);

#endif























