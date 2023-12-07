#ifndef _sequencerHandler_h_
#define _sequencerHandler_h_

#include <string.h>
#include <vector>
#include <bitset>     /*bitset*/
#include <math.h>

//define constants
#define SEQ_INSTRUCTION_WIDTH 32
#define SEQ_INST_SYMBOL_FIRST_BIT 0
#define SEQ_INST_SYMBOL_LAST_BIT 2
#define SEQ_MAX_INST_NUMBER 4
#define SEQ_END_SEQ_INST 4

#define SOFT_VAR_USE_POS_DELIMITER " "
#define SOFT_VAR_NAME_POSITION  0
#define SOFT_VAR_VALUE_POSITION 2

#define MULT_FOR_SYNC 20
#define LOOP_EXEC_TIME      6
#define DELAY_EXEC_TIME     8
#define STATUS_EXEC_TIME    5
#define END_EXEC_TIME       7
#define ENDSEQ_EXEC_TIME    5

#define SEQ_INSTRUCTION_WIDTH 32
#define SEQ_MAX_NUM_BITS_INST_VALUE 29
#define LTASOFT_MIN_SEQ_VAR_VALUE 1

#define SEQ_CONVENTION_CCD_COLS_NUMBER "CCDNCOL"
#define SEQ_CONVENTION_CCD_ROWS_NUMBER "CCDNROW"
#define SEQ_CONVENTION_CCD_PRESCAN_NUMBER "CCDNPRES"
#define SEQ_CONVENTION_READ_NSAMP_NUMBER "NSAMP"
#define SEQ_CONVENTION_READ_ROWS_NUMBER "NROW"
#define SEQ_CONVENTION_READ_COLS_NUMBER "NCOL"
#define SEQ_CONVENTION_INTERLEAVING "INTERLEAVING"
#define SEQ_CONVENTION_EXPOSURE_SECONDS "EXPOSURE"
#define SEQ_CONVENTION_TOGGLE_VDD "TOG_VDD"
#define SEQ_CONVENTION_TOGGLE_VR "TOG_VR"
#define SEQ_CONVENTION_TOGGLE_VDR "TOG_VDR"

const int LTASOFT_MAX_SEQ_VAR_VALUE = pow(2.0, SEQ_MAX_NUM_BITS_INST_VALUE) - 1;

using namespace std;

//opcode
//const char delayOC[] = "000";
//const char statusOC[] = "001";
//const char loopOC[]= "010";
//const char endLoopOC[]= "011";
//const char endSeqOC[] = "100";

const std::bitset<3> delayOC(0);
const std::bitset<3> statusOC(1);
const std::bitset<3> loopOC(2);
const std::bitset<3> endLoopOC(3);
const std::bitset<3> endSeqOC(4);

const unsigned int numOfBitsValue = 29;
const unsigned int maxNumBitsStatus = numOfBitsValue;
const int maxNumOfLoops = pow(2.0,numOfBitsValue)-1;
const int maxNumOfDelay = pow(2.0,numOfBitsValue)-1;


//
//"000"  -- delay Instruction
//"001"  -- setout Instruction
//"010" -- loadLR Instruction
//"011" -- DecLR and JNZ Instruction
//"100" -- END_SEQ Instruction

class sequencerHandler
{
    
    public:
    
    sequencerHandler(void);
    
    ~sequencerHandler(void);
    
    bool getSeqGood() {return seqGood;};

    int check_vars(vector <string> program, const int numInst,vector <string> &varName, vector <string> &varValue, vector <vector <int>> &varUsePos);
    
    int check_program(vector <string> program, const int numInst, vector<string> &inst);
    
    int getSeqInstruction(const string &inFileName, vector<string> &inst,vector <string> &varName, vector <string> &varValue, vector <vector <int>> &varUsePos);
    
    unsigned int getNumberOfSeqInstruction() {return compSeq.size();};
    
    int getInstruction(const unsigned int ind, string &instruction);
    
    string getSeqName() {return seqName;};

    int loadNewSequencer(const string seqFileName);
    
    int loadSequencer();
    
    int isSequencerVar(const string var, int &varInd);

    bool isSequencerVar(const string var);
    
    int getSequencerVarValue(const string var, string &varValue);
    
    int changeSeqVarValue(const string var, const string value);
    
    int check_is_a_number(string data);
    
    int getSeqVars(vector <string> &varName, vector <vector <int>> &varUsePos, vector <string> &varValue);


    protected :
///protected functions ///////////////////////////
bool fileExist(const char *fileName);


 int compileSequencer(const vector <string> &seqCont, vector <string> &compSeq, vector <string> &varName, vector <vector <int>> &varUsePos, vector <string> &varValue);
    
int loopFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const vector <string> &varsName, const vector <string> &varsValue, vector <vector <int> > &varUsePos, vector<unsigned long long> &loopInitAddress, unsigned int &loopIndex, string &outputLine, vector <unsigned long> &loopInTime, const int syncSeq);

int delayFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const vector <string> &varsName, const vector <string> &varsValue, vector <vector <int> > &varUsePos, const unsigned int &loopIndex, string &outputLine, vector <unsigned long> &loopInTime, const int syncSeq);

int endFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const vector <string> &varsName, const vector <string> &varsValue, vector<unsigned long long> &loopInitAddress, unsigned int &loopIndex, string &outputLine, vector <unsigned long> &loopInTime, const int syncSeq);

int endSyncFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const vector <string> &varsName, const vector <string> &varsValue, vector<unsigned long long> &loopInitAddress, unsigned int &loopIndex, string &outputLine, vector <unsigned long> &loopInTime, const int syncSeq);

int statusFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const vector <string> &varsName, const vector <string> &varsValue, const unsigned int &loopIndex, string &outputLine, vector <unsigned long> &loopInTime, const int syncSeq);
int endSeqFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const unsigned int loopIndex, string &outputLine);
    int addVar(const vector <string> &words, vector <string> &varsName, vector <string> &varsValue, vector <vector <int> > &varUsePos);
int orOperand(const string &prevVarValue, const string &valueAfterOperand, string &newVarValue);


    string seqName;
    vector <string> seqContOrig;
    vector <string> compSeqOrig;
    vector <string> varsNameOrig;
    vector <vector <int>> varsUsePosOrig;
    vector <string> varsValueOrig;
    vector<string> seqContent;
    vector<string> compSeq;
    vector <string> seqVarsName;
    vector <string> seqVarsValue;
    vector <vector <int>> seqVarsUsePos;
    bool procNewSeq;
    bool seqGood; //true if a valid sequencer is loaded
    

};

#endif
