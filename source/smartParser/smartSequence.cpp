#include <iostream>
#include "smartSequence.h"

using namespace std;

smartSequence::smartSequence()
{
}

void smartSequence::clearRecipes()
{
    recipes.clear();
}

void smartSequence::clearVariables()
{
    variables.clear();
}

void smartSequence::addRecipe(string name, smartRecipe &recipe)
{
    recipes.insert(make_pair(name, new smartRecipe(recipe)));
}

void smartSequence::addVariable(string name, smartVariable &variable)
{
    variables.insert(make_pair(name, new smartVariable(variable)));
}

void smartSequence::print()
{
    printVariables();
    printRecipes();
}

void smartSequence::printRecipes()
{
    smartRecipe *rec;
    map<string, smartRecipe*>::iterator it;
    for (it = recipes.begin(); it != recipes.end(); it++)
    {
        rec = it->second;
        rec->print();
    }
}

void smartSequence::printVariables()
{
    smartVariable *var;
    map<string, smartVariable*>::iterator it;
    for (it = variables.begin(); it != variables.end(); it++)
    {
        var = it->second;
        var->print();
        cout << endl;
    }
}

bool smartSequence::compile()
{
    // Compile variables.
    smartVariable *var;
    map<string, smartVariable*>::iterator it_var;
    for (it_var = variables.begin(); it_var != variables.end(); it_var++)
    {
        var = it_var->second;
        if ( !var->compile() )
        {
            return false;
        }
    }

    // Compile recipes.
    //smartRecipe *rec;
    //map<string, smartRecipe*>::iterator it;
    //for (it = recipes.begin(); it != recipes.end(); it++)
    //{
    //	rec = it->second;
    //	rec->compile();
    //}

    return true;
}

bool smartSequence::link()
{
    // link variables.
    smartVariable *var;
    map<string, smartVariable*>::iterator it_var;
    for (it_var = variables.begin(); it_var != variables.end(); it_var++)
    {
        var = it_var->second;
        if ( !var->link(&variables) )
        {
            return false;
        }
    }

    // Compile recipes.
    //smartRecipe *rec;
    //map<string, smartRecipe*>::iterator it;
    //for (it = recipes.begin(); it != recipes.end(); it++)
    //{
    //	rec = it->second;
    //	rec->compile();
    //}

    return true;
}
