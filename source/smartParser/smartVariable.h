#ifndef _smartVariable_h_
#define _smartVariable_h_

#include <string>
#include <map>

enum class varType {unknown, varLiteral, stateLiteral, stateReference};

enum class Status {init, uncompiled, compiled, linked, invalid};

using namespace std;

class smartVariable
{
    public:
        // Constructors.
        smartVariable();
        //smartVariable(const smartVariable &sv);

        // Functions.
        void set(string type, string name, string val);
        string name();
        unsigned long long value();
        void print();
        bool compile();
        bool link(map<string, smartVariable*>* varsVect);

        bool isCompiled();
        bool isLinked();

    private:
        Status compileState_;
        string typeStr_;
        string nameStr_;
        string valStr_;
        varType type_;
        unsigned long long valBin_;

};
#endif

