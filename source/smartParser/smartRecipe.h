#ifndef _smartRecipe_h_
#define _smartRecipe_h_

#include <string>
#include <vector>

#include "smartStep.h"

using namespace std;

class smartRecipe
{
    public:
        smartRecipe();
        smartRecipe(const smartRecipe &sr);

        void clearSteps(void);
        void addStep(smartStep &step);
        void setName(string name);
        string name();
        void print();
        const vector<smartStep*>& getSteps() const;

    private:
        string name_;

        // Vector of steps.
        vector<smartStep*> steps_;
        void printSteps();
};
#endif

