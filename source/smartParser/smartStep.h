#ifndef _smartStep_h_
#define _smartStep_h_

#include <string>

using namespace std;

class smartStep
{
    public:
        smartStep();
        smartStep(const smartStep &ss);
        void set(string state, string delay);
        void print();

    private:
        string state_;
        string delay_;
};
#endif

