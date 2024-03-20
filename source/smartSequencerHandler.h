#ifndef _smartSequencerHandler_h_
#define _smartSequencerHandler_h_

#include <string.h>
#include <vector>
#include <map>
#include "tinyxml2.h"
#include <exception>
#include <bitset>

#define SMART_MAX_SIZE_ORDER_DEPEN 4
#define SMART_SIZE_RECIPE 10

using namespace std;

typedef struct {
    string value;
    string delay;
} state_t;

typedef struct pseudoRecipeUB_t{
    string recipeName;
    int deeperLevel_id;
    int selfLevel_id;
    int upperLevel_id;
    string times_to_execute;

    //Dinamic properties
    bool isDynamic;
    vector<string> ncluster;                // Number of pixels per element in X
    vector<int> id_depen; // Pointer to the dependence recipe

    pseudoRecipeUB_t():recipeName(""),deeperLevel_id(-1),selfLevel_id(-1), upperLevel_id(-1), times_to_execute(""), isDynamic(false)
    {
        ncluster.clear();
        id_depen.clear();
    };
    ~pseudoRecipeUB_t()
    {

    };

} pseudoRecipeUB_t;

typedef struct {
    int nColsAve;
    int nRowsAve;
    vector <unsigned int> nCols;
    vector<vector<int>> nSampsPixRow;
    int nRowsAll;
    unsigned int nColsAllMax;
} smartImageDimensions_t;

class sequencerException : public exception {
    public:
        sequencerException(const string& msg) : msg_(msg) {}
        const char * what () const noexcept {
            return msg_.c_str();
        }
    private:
        string msg_;
};

class smartSequencerHandler
{

    public:

        smartSequencerHandler(void);

        ~smartSequencerHandler(void);

        string getSeqName() {return seqName;};
        bool getSeqGood() {return seqGood;};

        map<string,string> getVars(){return fVars;};
        bool dynVarInUse(){return !fActiveDynVar.empty();};
        vector<string> getActiveDynVar(){return fDynVars[fActiveDynVar];};
        vector <pseudoRecipeUB_t> getRecipeUB(){return fRecipeUB;};

        vector<string> getPrefixedVars(const char* prefix);

        string resolveVar(const string valStrIn);
        long resolveIntVar(const string valStrIn);
        float resolveFloatVar(const string valStrIn);
        bitset<32> resolveState(const string valStr);

        void loadNewSequencer(const string seqFileName );

        bool isSequencerVar(const string var){return (fVars.count(var)!=0);};
        void changeSeqVarValue(const string var, const string value);

        void    printSequencer(bool verbatim);
        string  getSequencerContent(bool verbatim, bool escape_special_char);
        
        float calculateSequencerDuration(); //calculates and returns the number of seconds it will take to execute the seq
        float calculateSequencerTimeUntilFirstData(); //calculates and returns the number of seconds it will take to get the fist data package
        float getDuration() {return duration;}; //returns the previously calculated duration (calculation happens when sequencer is loaded or modified)
        float getTimeUntilData() {return timeUntilData;}; //returns the previously calculated timeUntilData (calculation happens when sequencer is loaded or modified)
        vector<string> makeCommandsToBoard();

        smartImageDimensions_t calculateSmartImageDimensions();

    protected :

        void loadVariables();
        void loadDynamicVariables();
        void loadRecipes();
        void loadSequence();
        int allocPseudoRecipe();
        void compilePseudoRecipe(const string recString, const string timesString, const char * nClusterString, const int recIndex);

        float calculatePseudoRecipeDuration(const pseudoRecipeUB_t &pR);

        vector<string> parseWords(const string &lineToParse, const char delimiter);

        float timeToDataPR(const pseudoRecipeUB_t &pR, bool &firstDataFound);
        float seqStepCount(int recInd, bool &firstDataFound, bool haltOnFistData);

        bool seqGood;

        float duration;
    	float timeUntilData;

        tinyxml2::XMLDocument sseqDoc;

        string seqName;

        //recipes: name->vector of (status,delay) pairs
        map<string,vector<state_t>> fRecipes;

        //non-dynamic variables: name->value
        map<string,string> fVars;

        //states: name->value
        map<string,string> fStates;

        //dynamic variables: name->vector of values
        map<string,vector<string>> fDynVars;

        //the name of the dynamic variable used (we only accept one for now)
        string fActiveDynVar;

        //compiled sequence
        vector <pseudoRecipeUB_t> fRecipeUB;

};

#endif
