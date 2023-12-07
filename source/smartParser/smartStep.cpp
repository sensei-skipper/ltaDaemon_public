#include <iostream>
#include "smartStep.h"

using namespace std;

smartStep::smartStep()
{
}


smartStep::smartStep(const smartStep &ss)
{
    this->state_ = ss.state_;
    this->delay_ = ss.delay_;
}

void smartStep::set(string state, string delay)
{
    this->state_ = state;
    this->delay_ = delay;
}

void smartStep::print()
{
    cout << "-> state: " << state_ << endl;
    cout << "-> delay: " << delay_ << endl;
}

