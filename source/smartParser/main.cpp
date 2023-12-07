#include <iostream>
#include "xmlParser.h"
#include "smartSequence.h"

using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cout << "Usage: " << argv[0] << " <file>" << endl;
    }
    else
    {
        string fileName(argv[1]);
        smartSequence seq;

        xmlParser parser;
        parser.parse(fileName, &seq);

        //seq.print();
        if (!seq.compile())
        {
            cout << "Could not compile sequencer." << endl;
        }

        //seq.print();

        if (!seq.link())
        {
            cout << "Could not link sequencer." << endl;
        }

        seq.print();
    }

    return 0;
}
