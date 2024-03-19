#include <string.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <cmath>
#include <stdlib.h>   /*strtol*/
#include <bitset>     /*bitset*/

#include <algorithm>

#include <iomanip>
#include <sys/resource.h>

#include "loguru.hpp" //logging

#include "smartSequencerHandler.h"
#include "helperFunctions.h"

#define SMART_NO_EXTRA_LEVEL_ID -1
const float kClkStepDuration = 6.667e-8; // in seconds. The clock runs at 15Mhz    


using namespace std;

//===============================================================================


smartSequencerHandler::smartSequencerHandler(void)
{
    seqName = "";
    seqGood = false;
    duration = -1;
    timeUntilData = -1;
}

smartSequencerHandler::~smartSequencerHandler(void)
{}

string smartSequencerHandler::resolveVar(const string valStrIn) {
    string valStr = valStrIn;
    while (fVars.count(valStr)!=0) {
        valStr = fVars[valStr];
    }
    return valStr;
}

long smartSequencerHandler::resolveIntVar(const string valStrIn) {
    string valStr = resolveVar(valStrIn);
    char* p;
    long value = strtol(valStr.c_str(), &p, 10);
    if (*p!='\0') {//string was not a valid integer
        throw(sequencerException("bad variable value: " + valStr));
    }
    return value;
}

float smartSequencerHandler::resolveFloatVar(const string valStrIn) {
    string valStr = resolveVar(valStrIn);
    return atof(valStr.c_str());
}

bitset<32> smartSequencerHandler::resolveState(const string valStr) {
    if (valStr.find_first_not_of("10") == string::npos) {//it's a bitstring
        return bitset<32>(valStr);
    }

    if (fStates.count(valStr)!=0) {//it's a state name
        return resolveState(fStates[valStr]);
    }

    vector<string> words = parseWords(valStr, '|');

    if (words.size()<2) {//it's not an or expression, so we weren't able to do anything
        throw(sequencerException("bad state value: " + valStr));
    }

    bool firstWord = true;
    bitset<32> resultBin;
    for (uint i=0; i<words.size(); i++) {
        bitset<32> binWord = resolveState(words[i]);
        if (firstWord) {
            resultBin = binWord;
        } else {
            resultBin |= binWord;
        }
        firstWord = false;
    } 

    return resultBin;
}

vector<string> smartSequencerHandler::getPrefixedVars(const char* prefix) {
    vector <string> matchingVars;
    for (map<string,string>::iterator it=fVars.begin(); it!=fVars.end(); ++it) {
        if (strncmp(prefix, it->first.c_str(), strlen(prefix))==0) {
            //printf("%s: %s\n", it->first.c_str(), it->second.c_str());
            matchingVars.push_back(it->first);
        }
    }
    return matchingVars;
}

void smartSequencerHandler::loadVariables()
{
    tinyxml2::XMLElement* variables;
    if((variables = sseqDoc.FirstChildElement("variables"))==NULL){
        throw(sequencerException("Missing \'variables\' in sequencer file."));
    }

    tinyxml2::XMLElement* var = variables->FirstChildElement();

    while (var != NULL) {
        const char * typeStr = var->Value();
        string nameStr = var->Attribute("name");
        string valStr = var->Attribute("val");

        if (strcmp(typeStr,"var")==0) {
            fVars[nameStr] = valStr;
        } else if (strcmp(typeStr,"state")==0) {
            fStates[nameStr] = valStr;
        }
        var = var->NextSiblingElement();
    }
}

void smartSequencerHandler::loadDynamicVariables()
{
    tinyxml2::XMLElement* vars = sseqDoc.FirstChildElement("dynamicVariables");
    if (vars == NULL){
        throw(sequencerException("Missing \'dynamicVariables\' in sequencer file."));
    }

    tinyxml2::XMLElement* var = vars->FirstChildElement();

    while (var != NULL) {
        string name = var->Attribute("name");
        string vals = var->Attribute("vals");

        fDynVars[name] = parseWords(vals, ',');

        var = var->NextSiblingElement();
    }
}

void smartSequencerHandler::loadRecipes()
{
    tinyxml2::XMLElement* recipes = sseqDoc.FirstChildElement("recipes");
    if (recipes == NULL) {
        throw(sequencerException("Missing \'recipes\' in sequencer file."));
    }

    tinyxml2::XMLElement* recipe = recipes->FirstChildElement();
    while (recipe != NULL) {
        string nameStr = recipe->Attribute("name");
        vector <state_t> steps;
        tinyxml2::XMLElement* step = recipe->FirstChildElement();
        while (step != NULL) {
            string stateStr = step->Attribute("state");
            string delayStr = step->Attribute("delay");
            steps.push_back({stateStr, delayStr});
            step = step->NextSiblingElement();
        }
        fRecipes[nameStr] = steps;

        recipe = recipe->NextSiblingElement();
    }
}

vector<string> smartSequencerHandler::parseWords(const string &lineToParse, const char delimiter)
{
    vector<string> words;

    const string whitespace = " \n\r\t";

    //read words
    string word;
    stringstream line(lineToParse); 

    // Tokenizing w.r.t. delimiter
    while(getline(line, word, delimiter)) 
    { 
        if (word.find_first_not_of(whitespace) == string::npos) {//empty, or all whitespace
            throw(sequencerException("empty word in input: " + lineToParse));
        }
        //trim whitespace
        trimString(word);

        words.push_back(word);
    } 

    return words;
}

int smartSequencerHandler::allocPseudoRecipe() {
    //add a new pseudorecipe
    fRecipeUB.emplace_back();
    return fRecipeUB.size()-1;
}

void smartSequencerHandler::compilePseudoRecipe(const string recString, const string timesString, const char * nClusterString, const int recIndex) {
    //check if recipe exists
    if (fRecipes.count(recString)==0) {
        throw(sequencerException("recipe does not exist: " + recString));
    }

    fRecipeUB[recIndex].recipeName = recString;

    if (fDynVars.count(timesString)!=0) {//it's a dynamic variable
        fRecipeUB[recIndex].times_to_execute = "-1";
        fRecipeUB[recIndex].isDynamic = true;
        if (dynVarInUse()) {//if we're already using a dynamic variable
            throw(sequencerException("multiple recipes with dynamic variables"));
        }
        fActiveDynVar = timesString;
        //parse dynamic variable usage
        vector <string> dynIndUpdateValue = parseWords(nClusterString, ',');
        int parentIndex = recIndex;
        for (uint i = 0; i<dynIndUpdateValue.size(); i++) {//look up the parents of this pseudorecipe, the dynamic var depends on them
            parentIndex = fRecipeUB[parentIndex].upperLevel_id;
            fRecipeUB[recIndex].ncluster.push_back(dynIndUpdateValue[i]);
            fRecipeUB[recIndex].id_depen.push_back(parentIndex);
        }
    } else {//it's either an integer or a non-dynamic variable reference
        fRecipeUB[recIndex].times_to_execute = timesString;
    }
}

//convert the XML <sequence> element into a recipe list formatted for the LTA
void smartSequencerHandler::loadSequence()
{
    tinyxml2::XMLElement* sequenceElement;
    if((sequenceElement = sseqDoc.FirstChildElement("sequence"))==NULL){
        throw(sequencerException("Missing \'sequence\' in sequencer file."));
    }

    tinyxml2::XMLElement* recipeElement = sequenceElement->FirstChildElement();
    if (recipeElement == NULL){
        throw(sequencerException("sequence element has no child element"));
    }
    bool doneWithElement = false;//does this element (and its children) already exist in the recipe list?
    int recIndex = SMART_NO_EXTRA_LEVEL_ID;//index of this recipe in the recipe list
    int motherIndex = SMART_NO_EXTRA_LEVEL_ID;//index of this recipe's mother
    int sisterIndex = SMART_NO_EXTRA_LEVEL_ID;//index of this recipe's previous sibling (if any)
    while (true) {
        if (!doneWithElement) {
            recIndex = allocPseudoRecipe();
            //const char * eleValue = recipeElement->Value();
            const char * name = recipeElement->Attribute("name");
            const char * n = recipeElement->Attribute("n");
            const char * ncluster = recipeElement->Attribute("ncluster");

            //initialize default pointers
            fRecipeUB[recIndex].selfLevel_id = SMART_NO_EXTRA_LEVEL_ID;
            fRecipeUB[recIndex].deeperLevel_id = SMART_NO_EXTRA_LEVEL_ID;
            fRecipeUB[recIndex].upperLevel_id = SMART_NO_EXTRA_LEVEL_ID;
            if (motherIndex!=SMART_NO_EXTRA_LEVEL_ID) {
                fRecipeUB[recIndex].upperLevel_id = motherIndex;
                if (fRecipeUB[motherIndex].deeperLevel_id==SMART_NO_EXTRA_LEVEL_ID)//don't overwrite the pointer if it exists --- we may be a sister
                    fRecipeUB[motherIndex].deeperLevel_id = recIndex;
            }
            if (sisterIndex!=SMART_NO_EXTRA_LEVEL_ID) {//create the link from this recipe's previous sister
                fRecipeUB[sisterIndex].selfLevel_id = recIndex;
            }

            compilePseudoRecipe(name,n,ncluster,recIndex);

            tinyxml2::XMLElement* daughterElement;
            if((daughterElement = recipeElement->FirstChildElement())!=NULL){//process the child
                recipeElement = daughterElement;
                motherIndex = recIndex;
                sisterIndex = SMART_NO_EXTRA_LEVEL_ID;
                doneWithElement = false;
                continue;
            }
        }
        tinyxml2::XMLElement* sisterElement;
        if((sisterElement = recipeElement->NextSiblingElement())!=NULL){//process the next sister
            recipeElement = sisterElement;
            //motherIndex is the same as this element's mother
            motherIndex = fRecipeUB[recIndex].upperLevel_id;
            sisterIndex = recIndex;
            doneWithElement = false;
            continue;
        }
        //this element and its children are processed, and there is no next sister: go up a level
        recipeElement = recipeElement->Parent()->ToElement();
        if (recipeElement == sequenceElement) {//this is the top element, we have processed the full sequence
            break;
        }
        //look up the parent's recIndex
        recIndex = fRecipeUB[recIndex].upperLevel_id;
        doneWithElement = true;//parent recipe already exists, we will just look for sisters and parents
    }
}

void smartSequencerHandler::changeSeqVarValue(const string var, const string value) {
    LOG_F(INFO, "changing variable value: %s = %s (previously %s)", var.c_str(), value.c_str(), fVars[var].c_str());
    fVars[var] = value;
    LOG_F(INFO, "==== new seq duration: %s ===)",    printTime(calculateSequencerDuration()).c_str());
    LOG_F(INFO, "==== new time until data: %s ===)", printTime(calculateSequencerTimeUntilFirstData()).c_str());
}

vector<string> smartSequencerHandler::makeCommandsToBoard()
{
    //commands to send to the LTA
    vector <string> commToBoard;

    vector<string> dynVarValue = fDynVars[fActiveDynVar];
    if (dynVarInUse()) //if we're using a dynamic variable
        for (uint i = 0; i<dynVarValue.size(); i++) {
            long value = resolveIntVar(dynVarValue[i]);
            stringstream cmd_dyn_vector_value;
            cmd_dyn_vector_value << "sek dynamicVector " << i << " " << value << "\r";
            commToBoard.push_back(cmd_dyn_vector_value.str());
            LOG_F(1, "%s", cmd_dyn_vector_value.str().c_str());
        }

    for (uint i = 0; i<fRecipeUB.size(); i++) {
        stringstream cmd_deeperLevel_id;
        cmd_deeperLevel_id << "sek recipe " << i << " daughter " << fRecipeUB[i].deeperLevel_id<< "\r";
        commToBoard.push_back(cmd_deeperLevel_id.str());
        LOG_F(1, "%s", cmd_deeperLevel_id.str().c_str());

        stringstream cmd_upperLevel_id;
        cmd_upperLevel_id << "sek recipe " << i << " mother " << fRecipeUB[i].upperLevel_id<< "\r";
        commToBoard.push_back(cmd_upperLevel_id.str());
        LOG_F(1, "%s", cmd_upperLevel_id.str().c_str());

        stringstream cmd_selfLevel_id;
        cmd_selfLevel_id << "sek recipe " << i << " sister " << fRecipeUB[i].selfLevel_id<< "\r";
        commToBoard.push_back(cmd_selfLevel_id.str());
        LOG_F(1, "%s", cmd_selfLevel_id.str().c_str());

        vector<state_t> recipeStates = fRecipes[fRecipeUB[i].recipeName];
        for (uint j=0; j<recipeStates.size(); j++) {
            stringstream cmd_states;
            bitset<32> state = resolveState(recipeStates[j].value);
            long delay = resolveIntVar(recipeStates[j].delay);
            cmd_states << "sek recipe " << i << " status " << j <<" "<< state.to_ulong() << " " <<delay<< "\r";
            commToBoard.push_back(cmd_states.str());
            LOG_F(1, "%s", cmd_states.str().c_str());
        }

        if (fRecipeUB[i].isDynamic) {
            stringstream cmd_dynamic;
            cmd_dynamic << "sek recipe " << i << " dynamic\r";
            commToBoard.push_back(cmd_dynamic.str());
            LOG_F(1, "%s", cmd_dynamic.str().c_str());
            for (uint j = 0; j<fRecipeUB[i].ncluster.size(); j++) {
                stringstream cmd_dynamic_array;
                long value = resolveIntVar(fRecipeUB[i].ncluster[j]);
                cmd_dynamic_array << "sek recipe " << i << " dynamicDependence " << j << " " << fRecipeUB[i].id_depen[j] << " " << value<< "\r";
                commToBoard.push_back(cmd_dynamic_array.str());
                LOG_F(1, "%s", cmd_dynamic_array.str().c_str());
            }
        } else {
            stringstream cmd_dynamic;
            cmd_dynamic << "sek recipe " << i << " dynamic " <<fRecipeUB[i].isDynamic <<"\r";
            commToBoard.push_back(cmd_dynamic.str());
            LOG_F(1, "%s", cmd_dynamic.str().c_str());

            long value = resolveIntVar(fRecipeUB[i].times_to_execute);
            stringstream cmd_times_to_execute;
            cmd_times_to_execute << "sek recipe " << i << " n " << value<< "\r";
            commToBoard.push_back(cmd_times_to_execute.str());
            LOG_F(1, "%s", cmd_times_to_execute.str().c_str());
        }
    }
    return commToBoard;
}

void smartSequencerHandler::printSequencer(){
    stringstream printstream;
    printstream << "VARIABLES: " << endl;
    printstream << "----------" << endl << endl;
    for (map<string,string>::iterator it=fVars.begin(); it!=fVars.end(); ++it) {
        printstream << it->first  << " " << it->second<< endl;
    }
    printstream << endl << endl;

    printstream << "STATES: " << endl;
    printstream << "----------" << endl << endl;
    for (map<string,string>::iterator it=fStates.begin(); it!=fStates.end(); ++it) {
        printstream << it->first  << " " << it->second<< endl;
    }
    printstream << endl << endl;

    printstream << "DYNAMIC VARIABLES: " << endl;
    printstream << "------------------" << endl << endl;
    for (map<string,vector<string>>::iterator it=fDynVars.begin(); it!=fDynVars.end(); ++it) {
        printstream <<it->first << "= [ ";
        for (uint j=0; j<it->second.size(); j++) {
            printstream << it->second[j] << " ";
        }
        printstream << " ] " << endl << endl;
    }
    printstream << "ACTIVE DYNAMIC VARIABLE: " << fActiveDynVar << endl << endl;

    printstream << "RECIPE DEFINITION: " << endl;
    printstream << "------------------" << endl << endl;
    for (map<string,vector<state_t>>::iterator it=fRecipes.begin(); it!=fRecipes.end(); ++it) {
        printstream << it->first << "= ";
        for (uint j=0; j<it->second.size(); j++) {
            printstream << it->second[j].value << "(" << it->second[j].delay << ") ";
        }
        printstream << endl;
    }
    printstream << endl << endl;

    printstream << "SEQUENCE DEFINITION: " << endl;
    printstream << "--------------------" << endl << endl;
    printstream << "   RECIPE STRUCT DEFINITION " << endl << endl;
    for (uint i = 0; i<fRecipeUB.size(); i++) {
        printstream << "\t ------ " << i << " -------"<< endl;
        printstream << "\t " << "recipeUB[i].deeperLevel_id \t\t" << fRecipeUB[i].deeperLevel_id << endl;
        printstream << "\t " << "recipeUB[i].selfLevel_id \t\t" << fRecipeUB[i].selfLevel_id << endl;
        printstream << "\t " << "recipeUB[i].upperLevel_id \t\t" << fRecipeUB[i].upperLevel_id << endl;
        printstream << "\t " << "recipeUB[i].recipeName \t\t" << fRecipeUB[i].recipeName << endl;
        printstream << "\t " << "recipeUB[i].times_to_execute \t\t" << fRecipeUB[i].times_to_execute << endl;
        printstream << "\t " << "recipeUB[i].isDynamic  \t\t" << fRecipeUB[i].isDynamic << endl;
        printstream << "\t " << "recipeUB[i].id_depen   \t\t" << fRecipeUB[i].id_depen.size() << endl;

        for (uint j=0; j<fRecipeUB[i].id_depen.size(); j++) {
            printstream <<fRecipeUB[i].id_depen[j] << ", ";
        }
        printstream << endl << "\t -----------------------------------------------------" << endl;
        printstream << endl;
    }
    LOG_F(1, "%s", printstream.str().c_str());
}

smartImageDimensions_t smartSequencerHandler::calculateSmartImageDimensions(){
    bool oneDynamicRecipeFlag = false;
    vector <int> xWidth;
    vector <int> yWidth;
    vector<int> motherRecipe;
    vector<int> motherTimeToExec;
    for (uint recInd = 0; recInd<fRecipeUB.size(); recInd++) {
        if(fRecipeUB[recInd].isDynamic){//check if recipe is dynamic
            if (!oneDynamicRecipeFlag) {//accepting only one dynamic variable for now
                oneDynamicRecipeFlag = true;
            } else {
                LOG_F(WARNING, "===========================================");
                LOG_F(WARNING, "WE ONLY MANAGE ONE DYNAMIC VARIABLE");
                LOG_F(WARNING, "NO SMART IMAGE CREATED");
                LOG_F(WARNING, "===========================================");
                throw sequencerException("too many dynamic variables");
            }

            //get size of the variable region size
            xWidth.push_back(resolveIntVar(fRecipeUB[recInd].ncluster[0]));
            yWidth.push_back(resolveIntVar(fRecipeUB[recInd].ncluster[1]));
            //find all the mothers
            int mother_id = fRecipeUB[recInd].upperLevel_id;
            while (mother_id!=SMART_NO_EXTRA_LEVEL_ID) {
                motherRecipe.push_back(mother_id);
                motherTimeToExec.push_back(resolveIntVar(fRecipeUB[mother_id].times_to_execute));
                mother_id = fRecipeUB[mother_id].upperLevel_id;
            }
            if (motherTimeToExec.size()!=2) {//we only accept 2D readouts
                LOG_F(WARNING, "===========================================");
                LOG_F(WARNING, "WE ONLY ACCEPT 2D SEQUENCES");
                LOG_F(WARNING, "NO SMART IMAGE CREATED");
                LOG_F(WARNING, "===========================================");
                throw sequencerException("not a 2D image");
            }
        }
    }
    //check if there is at least one dynamic variable in use
    if (!oneDynamicRecipeFlag) {
        LOG_F(WARNING, "===========================================");
        LOG_F(WARNING, "NO DYNAMIC VARIABLE IN USE IN THIS SEQUENCER");
        LOG_F(WARNING, "NO SMART IMAGE CREATED");
        LOG_F(WARNING, "===========================================");
        throw sequencerException("no dynamic variable in use");
    }

    //get the final number of columns and rows in the image
    smartImageDimensions_t dims;
    //int nPixReg = xWidth[0]*yWidth[0];
    dims.nColsAve = motherTimeToExec[0];
    dims.nRowsAve = motherTimeToExec[1];
    dims.nCols = vector<unsigned int>(dims.nRowsAve,0);//this vector holds number of columns for each row
    dims.nSampsPixRow = vector<vector<int>>(dims.nRowsAve,vector <int> (0,0));

    int nRegX = ceil((double) dims.nColsAve/ (double) xWidth[0]);

    for (int y = 0; y<dims.nRowsAve; y++) {
        int regIndY = y/yWidth[0];
        for (int x = 0; x<dims.nColsAve; x++) {
            int regIndX = x/xWidth[0];
            int regInd = regIndX + regIndY*nRegX;
            long dynVarVal = resolveIntVar(getActiveDynVar()[regInd]);
            dims.nCols[y] += dynVarVal;
            dims.nSampsPixRow[y].push_back(dynVarVal);
            //cout << "nSampsPixRow[y][x] " << nSampsPixRow[y][x];
        }
    }
    dims.nRowsAll = dims.nRowsAve;
    dims.nColsAllMax = *std::max_element(&(dims.nCols[0]),&(dims.nCols[0]) + dims.nCols.size());
    for (uint v=0;v<dims.nCols.size();v++) {
        LOG_F(1, "nCols[%d] %d", v, dims.nCols[v]);
    }	
    return dims;
}

void smartSequencerHandler::loadNewSequencer(const string seqFileName ){

    if(sseqDoc.LoadFile( seqFileName.c_str() ) != 0){
        sseqDoc.Print();
        throw(sequencerException("Can't read sequencer file: " + seqFileName));
    }

    //    tinyxml2::XMLPrinter printer;
    //    sseqDoc.Print( &printer );
    //    cout <<printer.CStr() << endl;

    //reset local variables
    seqGood = false;
    fRecipes.clear();
    fVars.clear();
    fStates.clear();
    fDynVars.clear();
    fActiveDynVar.clear();

    //reset output variables
    fRecipeUB.clear();

    //translate variables
    loadVariables();

    //translate dynamic array
    loadDynamicVariables();

    //tranlate recipes
    loadRecipes();

    //tranlate sequence
    loadSequence();

    printSequencer();

    //calculate the time it will take to execute the seq
    calculateSequencerDuration();

    //calculate the time until the first data package is sent
    calculateSequencerTimeUntilFirstData();

    //if we make it here without a sequencerException, we are good
    seqGood = true;
}


float smartSequencerHandler::calculateSequencerDuration(){

    stringstream printstream;
    printstream << "\n============================\n";
    printstream << "Calculation of seq duration:\n";
    float totalSeqDuration = 0;
    for (uint recInd = 0; recInd<fRecipeUB.size(); recInd++) {

        int totalTimesToExecute = resolveIntVar(fRecipeUB[recInd].times_to_execute);
        int mother_id = fRecipeUB[recInd].upperLevel_id;
        int nestDepth = 0;
        while (mother_id!=SMART_NO_EXTRA_LEVEL_ID) {
            totalTimesToExecute *= resolveIntVar(fRecipeUB[mother_id].times_to_execute);
            mother_id = fRecipeUB[mother_id].upperLevel_id;
            nestDepth++;
        }
        std::string indentNest(nestDepth*5, ' ');
        float rT = calculatePseudoRecipeDuration(fRecipeUB[recInd]);
        printstream << indentNest << "("<< recInd +1 << ") " << fRecipeUB[recInd].recipeName << endl;
        printstream << indentNest << " -recipeDuration [s]:  " << rT*kClkStepDuration << endl;
        printstream << indentNest << " -totalTimesToExecute: " << totalTimesToExecute << endl;
        printstream << indentNest << " -totalDuration [s]:   " << totalTimesToExecute*rT*kClkStepDuration << endl;

        totalSeqDuration += totalTimesToExecute*rT;
    }

    printstream << "\nTotal duration: " << printTime(totalSeqDuration*kClkStepDuration) << endl;
    printstream << "============================\n";

    LOG_F(1, "%s", printstream.str().c_str());
    
    duration = totalSeqDuration*kClkStepDuration;
    return duration;
}

float smartSequencerHandler::calculatePseudoRecipeDuration(const pseudoRecipeUB_t &pR){

    std::vector<state_t> &recipe = fRecipes[pR.recipeName];
    float totalTime = 0;
    for (std::vector<state_t>::iterator it = recipe.begin(); it != recipe.end(); ++it)
    {
        totalTime += resolveFloatVar(it->delay);
    }
    return totalTime;
}


float smartSequencerHandler::timeToDataPR(const pseudoRecipeUB_t &pR, bool &firstDataFound){
    std::vector<state_t> &recipe = fRecipes[pR.recipeName];
    float rT = 0;
    for (std::vector<state_t>::iterator it = recipe.begin(); it != recipe.end(); ++it)
    {
        rT += resolveFloatVar(it->delay);
        if (fStates[it->value].find("HD2") != std::string::npos) {
            firstDataFound = true;
            break;
        }
    }
    return rT;
}


float smartSequencerHandler::seqStepCount(int recInd, bool &firstDataFound, bool haltOnFistData = true){

    if(firstDataFound==true && haltOnFistData==true) return 0;
    const pseudoRecipeUB_t &pR=fRecipeUB[recInd];
    
    float pT = timeToDataPR(pR, firstDataFound);

    float inRT = 0;
    if(pR.deeperLevel_id!=SMART_NO_EXTRA_LEVEL_ID){
        pT += seqStepCount(pR.deeperLevel_id, firstDataFound);    
    }

    int timesToExecute = ((firstDataFound==true && haltOnFistData==true) ? 1 : resolveIntVar(fRecipeUB[recInd].times_to_execute));
    pT *= timesToExecute;

    if(pR.selfLevel_id!=SMART_NO_EXTRA_LEVEL_ID){
        pT += seqStepCount(pR.selfLevel_id, firstDataFound);
    }
    
    return pT;
}


float smartSequencerHandler::calculateSequencerTimeUntilFirstData(){

    bool firstDataFound = false;
    const float timeToFirstData = seqStepCount(0, firstDataFound) *kClkStepDuration;
    
    stringstream printstream;
    printstream << "\n============================\n";
    printstream << "Calculation of time until first data:\n";
    printstream << "\nTime until first data: " << printTime(timeToFirstData) << endl;
    printstream << "============================\n";

    LOG_F(1, "%s", printstream.str().c_str());
    
    timeUntilData = timeToFirstData;
    return timeUntilData;
}


