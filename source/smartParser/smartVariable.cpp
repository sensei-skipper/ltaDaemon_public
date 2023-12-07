#include <iostream>
#include <regex>
#include "smartVariable.h"

#define STATE_WIDTH 29

using namespace std;

smartVariable::smartVariable()
{
    compileState_ = Status::init;
    type_ = varType::unknown;
    typeStr_ = "";
    nameStr_ = "";
    valStr_ = "";
    valBin_ = 0;
}

//smartVariable::smartVariable(const smartVariable &sv)
//{
    //this->compileState_ = Status::init;
    //type_ = varType::unknown;
    //this->typeStr_ = sv.typeStr_;
    //this->nameStr_ = sv.nameStr_;
    //this->valStr_ = sv.valStr_;
//}

void smartVariable::set(string type, string name, string val)
{
    compileState_ = Status::uncompiled;
    this->typeStr_ = type;
    this->nameStr_ = name;
    this->valStr_ = val;
}

string smartVariable::name()
{
    return this->nameStr_;
}

unsigned long long smartVariable::value()
{
    return this->valBin_;
}

void smartVariable::print()
{
    cout << "Variable \"" << nameStr_ << "\": " << endl;
    cout << "-> Type: " << typeStr_ << endl;
    cout << "-> Val: " << valStr_ << endl;
    if (compileState_ == Status::linked) {
        switch (type_) {
            case varType::varLiteral:
                cout << "-> Val (linked): " << valBin_ << endl;
                break;
            case varType::stateLiteral:
            case varType::stateReference:
                cout << "-> Val (linked): " << bitset<STATE_WIDTH>(valBin_) << endl;
                break;
        }
    }
}

bool smartVariable::compile()
{
    regex rStateLiteral("[01]+");
    regex rStateReference("(\\s*[a-zA-Z0-9]+\\s*\\|+\\s*)+[a-zA-Z0-9]+\\s*");
    regex rVarLiteral("(\\s*[0-9]+\\s*)");
    if (compileState_ == Status::init) {//values were never set
        cout << "variable was not set" << endl;
        return false;
    }
    // state type.
    if ( typeStr_.compare("state") == 0 )
    {
        // Check if it is a "numeric" state: 0011010011 ...
        if (regex_match(valStr_, rStateLiteral))
        {
            unsigned long long valBin = std::bitset<STATE_WIDTH>(valStr_).to_ullong();
            this->valBin_ = valBin;

            // Update compileState.
            compileState_ = Status::linked;
            type_ = varType::stateLiteral;
            return true;
        }

        // Check if it is a "or" state: H1 | V1 | SW ...
        else if (regex_match(valStr_, rStateReference))
        {
            // Update compileState.
            compileState_ = Status::compiled;
            type_ = varType::stateReference;
            return true;
        }

        // Not a valid format.
        else
        {
            cout << "Error compiling " << typeStr_ << " " << nameStr_ << ": " << valStr_ << endl;
            compileState_ = Status::invalid;
            type_ = varType::unknown;
            return false;
        }
    }

    // var type.
    else if ( typeStr_.compare("var") == 0 )
    {
        if (regex_match(valStr_, rVarLiteral))
        {
            unsigned long long valBin = atoi(valStr_.c_str());
            this->valBin_ = valBin;

            // Update compileState.
            compileState_ = Status::linked;
            type_ = varType::varLiteral;
            return true;
        }
    }

    // not a valid type.
    else
    {
        compileState_ = Status::invalid;
        type_ = varType::unknown;
        cout << "Error compiling " << typeStr_ << " " << nameStr_ << ": " << valStr_ << endl;
        cerr << "Not a valid type for variable " << nameStr_ << endl;
        return false;
    }
}

bool smartVariable::link(map<string, smartVariable*>* varsVect)
{
    // Check if variable is already linked.
    if ( isLinked() ) return true;

    // Check if variable is compiled.
    if ( !isCompiled() )
    {
        cout << "Variable " << nameStr_ << " must be compiled before linking." << endl;
        return false;
    }

    // Iterate through matches.
    regex rWord("([a-zA-Z0-9]+)");
    regex_iterator<string::iterator> rit (valStr_.begin(), valStr_.end(), rWord);
    regex_iterator<string::iterator> rend;

    // Result.
    unsigned long long res = 0;

    switch (type_) {
        case varType::varLiteral:
        case varType::stateLiteral:
            compileState_ = Status::linked;//it's already done
            return true;
        case varType::stateReference:
            // Execute operation.
            while (rit != rend)
            {
                // Get variable name.
                string varName = rit->str();

                if (varsVect->count(varName)==0){
                    cout << "Error linking " << typeStr_ << " " << nameStr_ << ": variable " << varName << " not found." << endl;
                    compileState_ = Status::invalid;
                    return false;
                }

                // Search variable in list.
                smartVariable* vv = varsVect->at(varName);

                //recursively link referenced variables - this should not be needed as long as all referenced varibles are literals
                //if (!vv->isLinked()) vv->link(varsVect);

                // Check if variable is compiled.
                if ( vv->isLinked() )
                {
                    res |= vv->value();
                }
                else
                {
                    cout << "Error linking " << typeStr_ << " " << nameStr_ << ": variable " << varName << " not compiled." << endl;
                    compileState_ = Status::invalid;
                    return false;
                }

                rit++;
            }

            // If it reaches this point, link was successfull.
            valBin_ = res;
            // Update compileState.
            compileState_ = Status::linked;
            return true;
        default:
            cout << "No var type, or invalid var type" << endl;
            compileState_ = Status::invalid;
            return false;
    }
}

bool smartVariable::isCompiled()
{
    return (compileState_==Status::compiled || compileState_==Status::linked);
}

bool smartVariable::isLinked()
{
    return (compileState_==Status::linked);
}

