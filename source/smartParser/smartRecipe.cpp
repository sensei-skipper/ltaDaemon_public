#include <iostream>
#include "smartRecipe.h"

using namespace std;

smartRecipe::smartRecipe()
{
}

smartRecipe::smartRecipe(const smartRecipe &sr)
{
    // Copy name.
    this->name_ = sr.name_;

    // Copy steps.
    vector<smartStep*> steps = sr.getSteps();
    vector<smartStep*>::iterator it;
    for (it = steps.begin(); it != steps.end(); it++)
    {
        this->addStep(*(*it));
    }
}

void smartRecipe::clearSteps()
{
    steps_.clear();
}

void smartRecipe::addStep(smartStep &step)
{
    steps_.push_back(new smartStep(step));
}

void smartRecipe::setName(string name)
{
    this->name_ = name;
}

string smartRecipe::name()
{
    return this->name_;
}

void smartRecipe::print()
{
    cout << "Recipe \"" << name_ << "\":" << endl;
    printSteps();
    cout << endl;
}

void smartRecipe::printSteps()
{
    vector<smartStep*>::iterator it;
    for (it = steps_.begin(); it != steps_.end(); it++)
    {
        (*it)->print();
    }
}

const vector<smartStep*>& smartRecipe::getSteps() const
{
    return steps_;
}

