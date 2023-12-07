#ifndef _smartSequence_h_
#define _smartSequence_h_

#include <string>
#include <map>

#include "smartRecipe.h"
#include "smartVariable.h"

using namespace std;

class smartSequence
{
    public:
        smartSequence();

        void clearRecipes(void);
        void clearVariables(void);
        void addRecipe(string name, smartRecipe &recipe);
        void addVariable(string name, smartVariable &variable);
        void print();

        bool compile();
        bool link();

    private:
        // Vector of recipes.
        map<string, smartRecipe*> recipes;

        // Vector of variables.
        map<string, smartVariable*> variables;

        void printRecipes();
        void printVariables();
};
#endif

