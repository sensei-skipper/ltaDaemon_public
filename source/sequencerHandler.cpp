#include <string.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <cmath>
#include <stdlib.h>   /*strtol*/

#include <iomanip>
#include <sys/resource.h>

#include "sequencerHandler.h"

#include "loguru.hpp" //logging

using namespace std;

int gVerbosity = true;

//===============================================================================


sequencerHandler::sequencerHandler(void)
{
    seqGood = false;
}

sequencerHandler::~sequencerHandler(void)
{}

///////////////////////////////////////////////////////////////////////////////////////

//int sequencerHandler::check_vars(vector <string> program, const int numInst,vector <string> &varName, vector <string> &varValue, vector <vector <int>> &varUsePos)
//{
//
//    for (int i = numInst-1; i < program.size(); ++i)
//    {
//        //separate srting in entities
//        string line = program[i];
//        vector <string> words;
//        string word;
//        stringstream lineStringStream(line);
//        string whiteSpace(SOFT_VAR_USE_POS_DELIMITER);
//        //while ( getline(lineStringStream,word,SOFT_VAR_USE_POS_DELIMITER) ) {
//        while ( getline(lineStringStream,word,*(whiteSpace.c_str()))){
//            words.push_back(word);
//        }
//
//        //save variable name and position where it is used
//        //check if all the positions of that variable are numeric and possibles
//        //saved the position
//        vector <int> pos;
//        for (int j = SOFT_VAR_VALUE_POSITION + 1; j < words.size(); ++j)
//        {
//            if (words[j].find_first_not_of( "0123456789" ) != string::npos)
//            {
//                if (gVerbosity){cerr <<  "bad expresion for position of the use of variables in sequencer" << endl;}
//                cout << "esto es la que da error " << words[j] << endl;
//                return -2;
//            }
//            int position = atoi(words[j].c_str());
//            if (position > numInst-1)
//            {
//                if (gVerbosity){cerr <<  "used position of variables in sequencer larger than sequencer size" << endl;}
//                return -2;
//            }
//            pos.push_back(position);
//        }
//        if (pos.size()>0)
//        {
//            varName.push_back(words[SOFT_VAR_NAME_POSITION]);
//            varValue.push_back(words[SOFT_VAR_VALUE_POSITION]);
//            varUsePos.push_back(pos);
//        }
//
//    }
//
//    return 0;
//}
//
//int sequencerHandler::check_program(vector <string> program, const int numInst, vector<string> &inst){
//
//    unsigned long symbULong = 0;
//    for (int i = 0; i < numInst; ++i)
//    {
//        //check if the number of characters in the instruction is ok
//        if (program[i].size()!=SEQ_INSTRUCTION_WIDTH){
//            if (gVerbosity){cerr << "sequecer has instructions with more bits than allowed!!" << normal << endl;}
//            return -2;
//        }
//        //check if all the characters are "1" or "0"
//        for (int j = 0; j < program[i].size(); ++j)
//        {
//            if (program[i].compare(j,1,"0")!=0 & program[i].compare(j,1,"1")!=0){
//                if (gVerbosity){cerr << "there are unknown symbols in sequencer"<< endl;}
//                return -2;
//            }
//        }
//        //check if the symbol of the instruction is real
//        bitset<SEQ_INST_SYMBOL_LAST_BIT-SEQ_INST_SYMBOL_FIRST_BIT+1> instBitset (program[i].substr(SEQ_INST_SYMBOL_FIRST_BIT,SEQ_INST_SYMBOL_LAST_BIT-SEQ_INST_SYMBOL_FIRST_BIT+1));
//        symbULong = instBitset.to_ulong();
//        if (symbULong > SEQ_MAX_INST_NUMBER){
//            if (gVerbosity){cerr  << "instruction symbol not recognized" << endl;}
//            return -2;
//        }
//
//        //translate binary format of instraction to decimal
//        string instString = program[i];
//        bitset<SEQ_INSTRUCTION_WIDTH> cmdBitset (instString);
//        unsigned long cmdLong = cmdBitset.to_ulong();
//
//        //output
//        stringstream translate;
//        translate << cmdLong;
//        string cmdString;
//        translate >> cmdString;
//        inst.push_back(cmdString);
//    }
//
//    return 0;
//}

//int sequencerHandler::getSeqInstruction(const string &inFileName, vector<string> &inst,vector <string> &varName, vector <string> &varValue, vector <vector <int>> &varUsePos) {
//    //open input file
//    ifstream infileStream (inFileName.c_str(),ios::in);
//    if (!infileStream.is_open()) {
//        cerr << "Error reading the file!!! Does it exist?" << normal <<endl;
//        cout << inFileName.c_str() << endl;
//        return 1;
//    }
//
//    //read lines
//    vector <string> program;
//    unsigned int lineInd = 0;
//    string line;
//    int checkStatus = 0;
//    unsigned long symbULong = 0;
//    int lastInstIndex = -1;
//    bool endOfSequenceFlag = false;
//    while ( getline (infileStream,line) )
//    {
//        //read line and put it on string program
//        program.push_back(line);
//        if (!endOfSequenceFlag)
//        {
//
//            bitset<SEQ_INST_SYMBOL_LAST_BIT-SEQ_INST_SYMBOL_FIRST_BIT+1> instBitset (line.substr(SEQ_INST_SYMBOL_FIRST_BIT,SEQ_INST_SYMBOL_LAST_BIT-SEQ_INST_SYMBOL_FIRST_BIT+1));
//            symbULong = instBitset.to_ulong();
//            if (symbULong == SEQ_END_SEQ_INST){
//                lastInstIndex = lineInd;
//                endOfSequenceFlag = true;
//            }
//            lineInd += 1;
//        }
//    }
//
//    int numInst = lastInstIndex + 1;
//    //check if there is at least one instruction
//    if (lastInstIndex<0) {
//        if (gVerbosity){
//            cout << "there is no instruction in the sequencer file" << endl;
//        }
//        return -1;
//    }
//
//    //check instructions and save them
//    checkStatus = check_program(program, numInst, inst);
//    if (checkStatus !=0) {
//        if (gVerbosity){
//            cerr << "sequencer is broken!!! do not use it!!!" << endl;
//        }
//        return -2;
//    }
//
//
//
//    //check variables and save them
//    checkStatus = check_vars(program, numInst, varName, varValue, varUsePos);
//    if (checkStatus !=0) {
//        if (gVerbosity){
//            cerr << "bad variables in sequencer, do not use it" << endl;
//        }
//        return -2;
//    }
//
//    //print program and used variables in the program
//    if (gVerbosity) {
//        cout << "PROGRAM: " << endl;
//        for (int i = 0; i < inst.size(); ++i)
//        {
//            cout << "sequencer[" << i << "]: " << inst[i] << endl;
//        }
//        cout << endl << "USED VARIABLES IN SEQUENCER: " << endl;
//        for (int i = 0; i < varName.size(); ++i)
//        {
//            cout << varName[i] << " = " << varValue[i] << " => positions: ";
//            for (int j = 0; j < varUsePos[i].size(); ++j)
//            {
//                cout << varUsePos[i][j] << " ";
//            }
//            cout << endl;
//        }
//
//    }
//    //close input file
//    infileStream.close();
//    return 0;
//
//}

int sequencerHandler::getInstruction(const unsigned int ind, string &instruction)
{
    instruction.clear();
    if (ind <= compSeq.size()-1) {
        instruction = compSeq[ind];
    } else {
        instruction = "";
        return 1;
    }
    return 0;
}

int sequencerHandler::loadNewSequencer(const string seqFileName)
{
    //check if file exists
    if (!fileExist(seqFileName.c_str())) return 1;
    
    //open input file and load it in internal variables
    ifstream infileStream (seqFileName.c_str(),ios::in);
    if (!infileStream.is_open()) {
        LOG_F(ERROR, "Error reading the file!!! Does it exist?");
        return 1;
    }
    vector <string> seqContTemp;
    string line;
    while ( getline (infileStream,line) )
    {
        seqContTemp.push_back(line);
    }
    //compiled loaded file from the RAM version
    vector <string> compSeqTemp;
    vector <string> varsNameTemp;
    vector <vector <int>> varsUsePosTemp;
    vector <string> varsValueTemp;
    procNewSeq = true;
    int status = 0;
    status = compileSequencer(seqContTemp, compSeqTemp, varsNameTemp, varsUsePosTemp, varsValueTemp);
    
    if (status != 0) {
        LOG_F(ERROR, "sequncer file does not compile, no sequencer loaded");
        return 1;
    }
    //sequencer is ok, I update sequencer in the program
    seqName = seqFileName;
    seqContent.clear();
    compSeq.clear();
    seqVarsName.clear();
    seqVarsValue.clear();
    seqVarsValue.clear();
    for (uint i=0; i < seqContTemp.size(); i++) seqContent.push_back(seqContTemp[i]);
    for (uint i=0; i < compSeqTemp.size(); i++) compSeq.push_back(compSeqTemp[i]);
    for (uint i=0; i < varsNameTemp.size(); i++) seqVarsName.push_back(varsNameTemp[i]);
    for (uint i=0; i < varsValueTemp.size(); i++) seqVarsValue.push_back(varsValueTemp[i]);
    for (uint i=0; i < varsUsePosTemp.size(); i++) seqVarsUsePos.push_back(varsUsePosTemp[i]);
    
    return 0;
}

int sequencerHandler::loadSequencer()
{
    //compile sequencer with the varName and value generated in run time
    vector <string> compSeqTemp;
    vector <string> varsNameTemp;
    vector <vector <int>> varsUsePosTemp;
    vector <string> varsValueTemp;
    procNewSeq = false;
    int status = 0;
    status = compileSequencer(seqContent, compSeqTemp, varsNameTemp, varsUsePosTemp, varsValueTemp);
    if (status !=0) {
        LOG_F(ERROR, "something wrong happened compiling with the new variable value!!");
        return 1;
    }
    for (uint i=0; i < compSeqTemp.size(); i++) compSeq[i] = compSeqTemp[i];
    
    return 0;
}

int sequencerHandler::isSequencerVar(const string var, int &varInd)
{
    
    for (uint i = 0; i < seqVarsName.size(); ++i){
        // If it is a sequencer variable responde true
        if (var.compare(seqVarsName[i].c_str()) == 0){
            varInd = i;
            return 0;
        }
    }
    return 1;
}

bool sequencerHandler::isSequencerVar(const string var)
{
    int dummy;
    return (isSequencerVar(var, dummy)==0);
}

int sequencerHandler::getSequencerVarValue(const string var, string &varValue)
{
    int varInd = 0;
    int status = isSequencerVar(var, varInd);
    if (status != 0) {
        return 1;
    }
    varValue = seqVarsValue[varInd];
    return 0;
}


int sequencerHandler::changeSeqVarValue(const string var, const string value)
{
    // Check if argument is a number.
    if (check_is_a_number(value) != 0)
    {
        LOG_F(ERROR, "setting for sequencer variable not numeric: %s - variable not modified", value.c_str());
        return 1;
    }
    
    // Convert to number.
    unsigned long tempVal = strtoul(value.c_str(), NULL, 10);
    
    // Check limits.
    if (tempVal > LTASOFT_MAX_SEQ_VAR_VALUE)
    {
        LOG_F(ERROR, "sequencer variable value above maximum limit, value: %lu (max. value = %d) - variable not modified", tempVal, LTASOFT_MAX_SEQ_VAR_VALUE);
        return 1;
    }
    if (tempVal < LTASOFT_MIN_SEQ_VAR_VALUE)
    {
        LOG_F(ERROR, "sequencer variable value below minimum limit, value: %lu (min. value = %d) - variable not modified", tempVal, LTASOFT_MIN_SEQ_VAR_VALUE);
        return 1;
    }
    
    int varInd = 0;
    int status = isSequencerVar(var, varInd);
    if (status != 0) {
        return 1;
    }
    
    seqVarsValue[varInd] = value;
    return 0;
}

int sequencerHandler::getSeqVars(vector <string> &varName, vector <vector <int>> &varUsePos, vector <string> &varValue)
{
    varName.clear();
    varUsePos.clear();
    varValue.clear();
    for (uint i=0; i < seqVarsName.size(); i++) varName.push_back(seqVarsName[i]);
    for (uint i=0; i < seqVarsValue.size(); i++) varValue.push_back(seqVarsValue[i]);
    for (uint i=0; i < seqVarsUsePos.size(); i++) varUsePos.push_back(seqVarsUsePos[i]);
    
    return 0;
}

int sequencerHandler::check_is_a_number(string data)
{
    for (uint i = 0; i < data.size(); ++i)
    {
        if (!isdigit((char) data[i])) return 1;
    }
    return 0;
}


int sequencerHandler::compileSequencer(const vector <string> &seqCont, vector <string> &compSeq, vector <string> &varsName, vector <vector <int>> &varsUsePos, vector <string> &varsValue)
{
    stringstream printstream;
    seqGood = false; //clear the seqGood flag, we'll set it to true if the sequencer compiles successfully
    //read lines
    stringstream outFileStream;
    vector <string> words;
    vector <unsigned long long> loopInitAddress;
    vector <unsigned long> loopInTime;
    unsigned int loopIndex = 0;
    string line;
    string word;
    char character;
    string whiteSpace(" ");
    int memoryIndex = 0;
    string outputLine;
    bool newOutputLine = false;
    for(uint indSeq = 0; indSeq<seqCont.size();indSeq++)
    {
        line = seqCont[indSeq];
        //replace transparent characters to white space
        words.clear();
        uint j = 0;
        while (j<line.size()) {
            character = line.at(j);
            if (character=='\t') {
                line.replace(j,1,whiteSpace);
                j++;
                continue;
            }
            if (character=='=') {
                line.replace(j,1," ");
                line.insert(j," =");
                j = j +2;
                continue;
            }
            if (character=='|') {
                line.replace(j,1," ");
                line.insert(j," |");
                j = j +2;
                continue;
            }
            if (character=='%') {
                line.replace(j,1," ");
                line.insert(j," %");
                j = j + 2;
                continue;
            }
            j++;
        }

        //get words from line
        stringstream lineStringStream(line);
        while ( getline(lineStringStream,word,*(whiteSpace.c_str())) ) {
            words.push_back(word);
        }


        //eliminate empty words
        uint i = 0;
        while ( i < words.size()) {
            if (words[i].size()==0) {
                words.erase(words.begin() + i);
            } else {
                i++;
            }
        }



        //interpret first word of the line
        int status = 0;
        if (words.size()>0) {

            //identify word
            if (words[0].compare(0,string::npos,"loop") == 0){
                status = loopFunc(words,outFileStream, memoryIndex, varsName, varsValue, varsUsePos, loopInitAddress, loopIndex, outputLine, loopInTime, 1);
                memoryIndex++;//increase memory address
                newOutputLine = true;

            } else if (words[0].compare(0,string::npos,"delay")==0){
                status = delayFunc(words,outFileStream, memoryIndex, varsName, varsValue, varsUsePos, loopIndex, outputLine, loopInTime, 1);
                memoryIndex++;//increase memory address
                newOutputLine = true;

            } else if (words[0].compare(0,string::npos,"end")==0) {
                status = endFunc(words,outFileStream, memoryIndex, varsName, varsValue, loopInitAddress,loopIndex, outputLine, loopInTime, 1);
                memoryIndex++;//increase memory address
                newOutputLine = true;

            } else if (words[0].compare(0,string::npos,"status")==0) {

                status = statusFunc(words,outFileStream, memoryIndex, varsName, varsValue, loopIndex, outputLine, loopInTime, 1);

                memoryIndex++;//increase memory address
                newOutputLine = true;

            } else if (words[0].compare(0,string::npos,"endsync")==0) {
                status = endSyncFunc(words,outFileStream, memoryIndex, varsName, varsValue, loopInitAddress,loopIndex, outputLine, loopInTime, 1);
                memoryIndex++;//increase memory address
                memoryIndex++;//increase memory address
                newOutputLine = true;

            } else if (words[0].compare(0,string::npos,"%")==0) {
                status = 0;

            } else if (words.size()>1) {
                if (words[1].compare(0,string::npos,"=")==0) {
                    status = addVar(words, varsName, varsValue, varsUsePos);
                }
            } else {
                LOG_F(ERROR, "unknown instruction: %s", words[0].c_str());
            }



            if (status != 0) {
                LOG_F(ERROR, "Something went wrong, please check source file!!!!");
                return 1;
            }

            //if verbosity plot output
            if (gVerbosity && newOutputLine) {
                printstream << memoryIndex - 1 << " => " <<outputLine << " := " << line << endl;
            }
            newOutputLine = false;
            outputLine.clear();


        }

    }

    int status = endSeqFunc(words,outFileStream, memoryIndex, loopIndex, outputLine);
    newOutputLine = true;
    if (status != 0) {
        LOG_F(ERROR, "Something went wrong, please check source file!!!!");
        return 1;
    }
    if (gVerbosity && newOutputLine) {
        printstream << memoryIndex << " => " <<outputLine << " := " << "EOF" << endl;
    }

    if (gVerbosity) {
        LOG_F(1, "%s", printstream.str().c_str());
    }

    //print used variables
    //delete outVhdlFileStream << "\n" << "\n";
    printstream.clear();
    printstream << endl << "Defined variables: " << endl;
    printstream << "size variable vector " << varsName.size() << " size variable position " << varsUsePos.size() << endl;
    for (uint i = 0; i < varsName.size(); ++i) {
        printstream << varsName[i] << " = " << varsValue[i] << endl;
        if (varsUsePos[i].size()>0){
            printstream << varsName[i] << " = " << varsValue[i];
            for (uint j = 0; j < varsUsePos[i].size(); ++j)
            {
                printstream << " " << varsUsePos[i][j];
            }
            printstream << "\n";
        }
    }
    LOG_F(1, "%s", printstream.str().c_str());

    //put stream in string vector
    string outLine;
    while ( getline (outFileStream,outLine) ) {
        compSeq.push_back(outLine);
    }

    seqGood = true;
    return 0;
}




//////////////////////////////////////////////////////////////////////////////////////
bool sequencerHandler::fileExist(const char *fileName){
    ifstream in(fileName,ios::in);

    if(in.fail()){
        LOG_F(ERROR,"Error reading file: %s\nThe file doesn't exist!", fileName);
        in.close();
        return false;
    }

    in.close();
    return true;
}

int sequencerHandler::loopFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const vector <string> &varsName, const vector <string> &varsValue, vector <vector <int> > &varUsePos, vector<unsigned long long> &loopInitAddress, unsigned int &loopIndex, string &outputLine, vector <unsigned long> &loopInTime, const int syncSeq){

    //check if the parameter is not a variable
    string varValue = words[1];
    long int value = strtol(varValue.c_str(),NULL,10);
    if (value <= 0) {//is a long integer
        for (uint i = 0; i<varsName.size(); i++) {
            if (varsName[i].compare(0,string::npos,varValue.c_str())==0) {
                if(procNewSeq) {
                    value = strtol(varsValue[i].c_str(),NULL,10);
                } else {
                    value = strtol(seqVarsValue[i].c_str(),NULL,10);
                }
                varUsePos[i].push_back(memoryIndex);
                break;
            }
        }
        if (value < 0) {
            LOG_F(ERROR, "Wrong number of loops: %s", words[1].c_str());
            return 1;
        }
    }

    if (value > maxNumOfLoops) { //param less than maximum
        LOG_F(ERROR, "Exceeded number of cycles: %s (max allowed number of cycles per loop is %d)", words[1].c_str(), maxNumOfLoops);
        return 1;
    }

    //integer to bin
    unsigned long long valueTemp = (unsigned long long) value;
    bitset <32> valueBin(valueTemp-1);//reduce one to eliminate the extra loop in the sequencer firmware by construction
    for (uint i=0; i<loopOC.size(); i++) {valueBin[valueBin.size()-loopOC.size()+i] = loopOC[i];}

    ostringstream outLine1, outLine2, outLine3;
    outLine1 << valueBin << '\n'; //line for the binary file
    outFileStream << outLine1.str();
    outLine2 << "0b" << valueBin << "," << '\n'; //line for vhdl file
    //delete outVhdlFileStream << outLine2.str();

    //delete uint32_t int2write = (uint32_t) valueBin.to_ulong();
    //delete outBinFileStream.write((char *)&int2write, sizeof(int2write));

    //if verbosity cout output line
    outputLine.clear();
    outputLine = outLine1.str();
    outputLine.erase(outputLine.size()-1);


    //save loop infomation in the stack
    loopInitAddress.push_back(memoryIndex + 1);
    loopIndex++;
    if (syncSeq==1) {
        loopInTime.push_back(0);
        loopInTime[loopIndex-1]+=LOOP_EXEC_TIME;
    }

    return 0;
}

int sequencerHandler::delayFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const vector <string> &varsName, const vector <string> &varsValue, vector <vector <int> > &varUsePos, const unsigned int &loopIndex, string &outputLine, vector <unsigned long> &loopInTime, const int syncSeq){

    //check if the parameter is not a variable
    string varValue = words[1];
    long int value = strtol(varValue.c_str(),NULL,10);
    if (value <= 0) {//is a long integer
        for (uint i = 0; i<varsName.size(); i++) {
            if (varsName[i].compare(0,string::npos,varValue.c_str())==0) {
                if(procNewSeq) {
                    value = strtol(varsValue[i].c_str(),NULL,10);
                } else {
                    value = strtol(seqVarsValue[i].c_str(),NULL,10);
                }
                varUsePos[i].push_back(memoryIndex);
                break;
            }
        }
        if (value <= 0) {
            LOG_F(ERROR, "Wrong number for delay: %s", words[1].c_str());
            return 1;
        }
    }

    if (value > maxNumOfDelay) { //param less than maximum
        LOG_F(ERROR, "Exceeded number for delay: %s (max allowed number for delay is %d)", words[1].c_str(), maxNumOfDelay);
        return 1;
    }

    //integer to bin
    unsigned long long valueTemp = (unsigned long long) value;
    bitset <32> valueBin(valueTemp);
    for (uint i=0; i<delayOC.size(); i++) {valueBin[valueBin.size()-delayOC.size()+i] = delayOC[i];}

    ostringstream outLine1, outLine2, outLine3;
    outLine1 << valueBin << '\n'; //line for the binary file
    outFileStream << outLine1.str();
    outLine2 << "0b" << valueBin << "," << '\n'; //line for vhdl file
    //delete outVhdlFileStream << outLine2.str();

    //delete uint32_t int2write = (uint32_t) valueBin.to_ulong();
    //delete outBinFileStream.write((char *)&int2write, sizeof(int2write));


    //if verbosity cout output line
    outputLine.clear();
    outputLine = outLine1.str();
    outputLine.erase(outputLine.size()-1);

    //add clock periods used to excecute this task by the sequencer
    //if sequencer call, add clock periods used to excecute this task by the sequencer
    if (syncSeq==1 && loopInTime.size()>0) {
        loopInTime[loopIndex-1]+=DELAY_EXEC_TIME;
        loopInTime[loopIndex-1]+=valueTemp;
    }


    return 0;

}

int sequencerHandler::endFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const vector <string> &varsName, const vector <string> &varsValue, vector<unsigned long long> &loopInitAddress, unsigned int &loopIndex, string &outputLine, vector <unsigned long> &loopInTime, const int syncSeq){

    if (loopIndex<=0) {
        LOG_F(ERROR, "check number of loop-end instructions!!!");
        return 1;
    }

    unsigned long long valueTemp = loopInitAddress[loopIndex-1];
    bitset <32> valueBin(valueTemp);
    for (uint i=0; i<endLoopOC.size(); i++) {valueBin[valueBin.size()-endLoopOC.size()+i] = endLoopOC[i];}

    ostringstream outLine1, outLine2, outLine3;
    outLine1 << valueBin << '\n'; //line for the binary file
    outFileStream << outLine1.str();
    outLine2 << "0b" << valueBin << "," << '\n'; //line for vhdl file
    //delete outVhdlFileStream << outLine2.str();

    //delete uint32_t int2write = (uint32_t) valueBin.to_ulong();
    //delete outBinFileStream.write((char *)&int2write, sizeof(int2write));

    //if verbosity cout output line
    outputLine.clear();
    outputLine = outLine1.str();
    outputLine.erase(outputLine.size()-1);

    //add clock periods used to excecute this task by the sequencer
    //if sequencer call, add clock periods used to excecute this task by the sequencer
    if (syncSeq==1 && loopInTime.size()>0) {
        loopInTime[loopIndex-1]+=END_EXEC_TIME;
    }

    //decrement loop index
    loopIndex--;
    loopInitAddress.pop_back();

    return 0;

}

int sequencerHandler::endSyncFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const vector <string> &varsName, const vector <string> &varsValue, vector<unsigned long long> &loopInitAddress, unsigned int &loopIndex, string &outputLine, vector <unsigned long> &loopInTime, const int syncSeq){

    if (loopIndex<=0) {
        LOG_F(ERROR, "check number of loop-end instructions!!!");
        return 1;
    }

    unsigned long long valueTemp = loopInitAddress[loopIndex-1];
    bitset <32> valueBin(valueTemp);
    for (uint i=0; i<endLoopOC.size(); i++) {valueBin[valueBin.size()-endLoopOC.size()+i] = endLoopOC[i];}


    //add clock periods used to excecute this task by the sequencer
    //if sequencer call, add clock periods used to excecute this task by the sequencer
    if (syncSeq==1 && loopInTime.size()>0) {
        loopInTime[loopIndex-1]+=END_EXEC_TIME;
    }
    //sum all the number of clocks in the sequencer nested in this loop
    unsigned long numClks = loopInTime[loopIndex-1] - LOOP_EXEC_TIME;
    for (uint i =loopIndex; i<loopInTime.size(); i++) {
        numClks += loopInTime[i];
    }
    LOG_F(1, "num clocks in the loop : %lu", numClks);

    unsigned long long magicNum = 0;
    int rest = numClks%MULT_FOR_SYNC;
    LOG_F(1, "remaining num of clocks : %d", rest);
    if (MULT_FOR_SYNC - rest >DELAY_EXEC_TIME) {
        magicNum = MULT_FOR_SYNC - rest - DELAY_EXEC_TIME;
    }else {
        magicNum = 2*MULT_FOR_SYNC - rest -DELAY_EXEC_TIME;
    }

    //add syncronizing delay to the total number of clocks in this loop
    if (syncSeq==1 && loopInTime.size()>0) {
        loopInTime[loopIndex-1] += magicNum + DELAY_EXEC_TIME;
    }

    //add delay to the output file!!!
    unsigned long long valueSyncDelay = magicNum;
    bitset <32> valueBinSyncDelay(valueSyncDelay);
    for (uint i=0; i<delayOC.size(); i++) {valueBinSyncDelay[valueBinSyncDelay.size()-delayOC.size()+i] = delayOC[i];}
    ostringstream outLineExtraDelay;
    outLineExtraDelay << valueBinSyncDelay << '\n'; //line for the binary file
    outFileStream << outLineExtraDelay.str();

    //add end line to compiled sequencer after the magic delay
    ostringstream outLine1, outLine2, outLine3;
    outLine1 << valueBin << '\n'; //line for the binary file
    outFileStream << outLine1.str();
    outLine2 << "0b" << valueBin << "," << '\n'; //line for vhdl file
    //delete outVhdlFileStream << outLine2.str();

    //delete uint32_t int2write = (uint32_t) valueBin.to_ulong();
    //delete outBinFileStream.write((char *)&int2write, sizeof(int2write));

    //if verbosity cout output line
    outputLine.clear();
    outputLine = outLine1.str();
    outputLine.erase(outputLine.size()-1);

    //decrement loop index
    loopIndex--;
    loopInitAddress.pop_back();

    return 0;

}

int sequencerHandler::statusFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const vector <string> &varsName, const vector <string> &varsValue, const unsigned int &loopIndex, string &outputLine, vector <unsigned long> &loopInTime, const int syncSeq){


    //check if the parameter is not a variable
    string varValue = words[1];
    char *endptr;
    long int value = strtol(varValue.c_str(),&endptr,2);
    if (value <= 0 && *endptr != '\0') {//is a long integer
        for (uint i = 0; i<varsName.size(); i++) {
            if (varsName[i].compare(0,string::npos,varValue.c_str())==0) {
                if(procNewSeq) {
                    value = strtol(varsValue[i].c_str(),&endptr,2);
                } else {
                    value = strtol(seqVarsValue[i].c_str(),&endptr,2);
                }
                break;
            }
        }
        if (value <= 0 && *endptr != '\0') {
            LOG_F(ERROR, "Wrong status value (status should have binary expression): %s", words[1].c_str());
            return 1;
        }
    }

    if (varValue.size() > maxNumBitsStatus) { //param less than maximum
        LOG_F(ERROR, "Exceeded number of bits for status: %s (max allowed number of bits is %d)", words[1].c_str(), maxNumBitsStatus);
        return 1;
    }

    //integer to bin
    unsigned long long valueTemp = (unsigned long long) value;
    bitset <32> valueBin(valueTemp);
    for (uint i=0; i<statusOC.size(); i++) {valueBin[valueBin.size()-statusOC.size()+i] = statusOC[i];}

    ostringstream outLine1, outLine2, outLine3;
    outLine1 << valueBin << '\n'; //line for the binary file
    outFileStream << outLine1.str();
    outLine2 << "0b" << valueBin << "," << '\n'; //line for vhdl file
    //delete outVhdlFileStream << outLine2.str();

    //delete uint32_t int2write = (uint32_t) valueBin.to_ulong();
    //delete outBinFileStream.write((char *)&int2write, sizeof(int2write));

    //if verbosity cout output line
    outputLine.clear();
    outputLine = outLine1.str();
    outputLine.erase(outputLine.size()-1);

    //if sequencer call, add clock periods used to excecute this task by the sequencer
    if (syncSeq==1 && loopInTime.size()>0) {
        loopInTime[loopIndex-1]+=STATUS_EXEC_TIME;
    }


    return 0;

}

int sequencerHandler::endSeqFunc(const vector <string> &words, stringstream &outFileStream, const int &memoryIndex, const unsigned int loopIndex, string &outputLine){

    unsigned long long valueTemp = 0;
    bitset <32> valueBin(valueTemp);
    for (uint i=0; i<endSeqOC.size(); i++) {valueBin[valueBin.size()-endSeqOC.size()+i] = endSeqOC[i];}

    ostringstream outLine1, outLine2, outLine3;
    outLine1 << valueBin << '\n'; //line for the binary file
    outFileStream << outLine1.str();
    outLine2 << "0b" << valueBin << '\n'; //line for vhdl file
    //delete outVhdlFileStream << outLine2.str();

    //delete uint32_t int2write = (uint32_t) valueBin.to_ulong();
    //delete outBinFileStream.write((char *)&int2write, sizeof(int2write));

    //check loops integrity
    if (loopIndex!=0) {
        LOG_F(ERROR, "check number of loop-end instructions!!!");
    }

    //if verbosity cout output line
    outputLine.clear();
    outputLine = outLine1.str();
    outputLine.erase(outputLine.size()-1);

    return 0;

}

int sequencerHandler::addVar(const vector <string> &words, vector <string> &varsName, vector <string> &varsValue, vector <vector <int> > &varUsePos){

    //chequear que la variable no existe
    string newVarName = words[0];
    bool varAlreadyExists = false;
    size_t varIndex = 0;
    for (uint i = 0; i<varsName.size(); i++) {
        if (varsName[i].compare(0,string::npos,newVarName.c_str())==0) {
            varAlreadyExists = true;
            varIndex = i;
            //varsValue[i] = newVarValue;
        }
    }

    // calculate value of the variable
    string prevVarValue = words[2];
    string newVarValue = words[2];
    for (uint i = 0; i<varsName.size(); i++) {
        if (varsName[i].compare(0,string::npos,words[2].c_str())==0) {
            prevVarValue = varsValue[i];
            newVarValue = varsValue[i];
        }
    }
    string valAfterOperand;
    size_t i = 2;
    while (i<words.size()) {
        //look for operands
        if (words[i].compare(0,string::npos,"%")==0) {
            break;
        }
        if(words[i].compare(0,string::npos,"|")==0) {


            //chequear si el valor anterior y el siguiente son variables!!
            for (uint j = 0; j<varsName.size(); j++) {
                if (varsName[j].compare(0,string::npos,words[i+1].c_str())==0) {
                    valAfterOperand = varsValue[j];
                    break;
                }
            }
            orOperand(prevVarValue, valAfterOperand, newVarValue);

            i++;
        }
        prevVarValue = newVarValue;
        i++;
    } //while, looking for operands

    //load value to variable
    if (!varAlreadyExists) {
        varsName.push_back(newVarName);
        varsValue.push_back(newVarValue);
        vector <int> emptyIntVec;
        varUsePos.push_back(emptyIntVec);
    } else {
        varsName[varIndex] = newVarValue;
    }

    return 0;

}


int sequencerHandler::orOperand(const string &prevVarValue, const string &valueAfterOperand, string &newVarValue) {
    //perform and operation beteen word[i-1] and word [i+1]
    //transform string to bits
    bitset<100> firstBinWord(prevVarValue);
    bitset<100> secondBinWord(valueAfterOperand);
    bitset<100> resultBin;
    resultBin = firstBinWord | secondBinWord;

    string resultString(resultBin.to_string());
    //biggest input word size
    size_t maxWordSize = 0;
    maxWordSize = (prevVarValue.size()>valueAfterOperand.size())? prevVarValue.size():valueAfterOperand.size();
    //final result with right number of bits
    newVarValue = resultString.substr(resultString.size()-maxWordSize);

    return 0;
}

