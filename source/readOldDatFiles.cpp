#include <vector>
#include <string.h>
#include <unistd.h>

#include "loguru.hpp" //logging

#include "dataConversion.h"
#include "helperFunctions.h"

using namespace std;

int main(int argc, char* argv[])
{
    loguru::init(argc, argv);
    loguru::set_thread_name("readOldDatFiles");
    loguru::add_file("readOldDatFiles.log", loguru::Append, loguru::Verbosity_MAX);
    if (argc!=6) {
        LOG_F(ERROR,"%s nsamp ncol nrow outname inname",argv[0]);
        return 0;
    }

    uint nSamp = atoi(argv[1]);
    uint nCols = atoi(argv[2]);
    uint nRows = atoi(argv[3]);
    string outFileName = argv[4];
    LOG_F(INFO,"%d nsamp %d cols %d rows",nSamp,nCols,nRows);
    vector<string> inFileNames;
    char tempstr[200];
    int i = 0;
    while (true) {
        sprintf(tempstr,"%s%d.dat",argv[5],i);
        if (fileExist(tempstr)) {//if file exists
            inFileNames.push_back(string(tempstr));
            LOG_F(INFO,"found %s",inFileNames[i].c_str());
        } else {
            //LOG_F(INFO,"did not find %s",tempstr);
            break;
        }
        i++;
    }
    if (inFileNames.empty()) {
        LOG_F(ERROR, "found no input files, exiting");
        return 0;
    }
    vector<imageVar> vars;

    vars.push_back({"NSAMP", to_string(nSamp), ""});
    vars.push_back({"NCOL", to_string(nCols), ""});
    vars.push_back({"NROW", to_string(nRows), ""});

    DataConversion data_;
    data_.setKeepTemp(1);//don't delete the .dat files
    data_.setDeleteDats(0);//don't delete the .dat files
    data_.setWriteTimestamps(0);//don't write timestamps
    data_.setInFileNames(&inFileNames);
    data_.bin_to_fits(outFileName, outFileName, nSamp*nCols, nRows, 0, vars);
}


