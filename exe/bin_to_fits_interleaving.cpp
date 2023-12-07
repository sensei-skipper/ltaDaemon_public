#include <string.h>
#include <stdio.h>
#include "fitsio.h"

#include <iostream>
#include <sstream>
#include <map>

#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include <fstream>
#include <unistd.h>
#include <getopt.h>    /* for getopt_long; standard getopt is in unistd.h */
#include <vector>
#include <cstdio>
#include <bitset>
#include <string.h>

#include <stdlib.h>   /*strtol*/
#include <bitset>     /*bitset*/

#include <climits>
#include <cmath>
#include <iomanip>
#include <sys/resource.h>

#include "TFile.h"
#include "TTree.h"
#include "TBranch.h"

#include "globalConstants.h"


//define constants
#define MAX_NUMBER_DATA_IN_UDP_PACK 255

using namespace std;

struct dataVars  {
    
    Short_t cntr;
    Short_t hdr;
    Short_t id;
    Int_t data;
    
    dataVars(): cntr(0), hdr(0), id(0), data(0)
    {
    };
    ~dataVars()
    {
    };
};

struct processingInfo {
    
    long nCols;
    long fileLength;
    
    processingInfo(): nCols(-1.), fileLength(1)
    {
    };
    ~processingInfo()
    {
    };
};

int deleteFile(const char *fileName) {
    cout << yellow;
    cout << "Will overwrite: " << fileName << endl << endl;
    cout << normal;
    return unlink(fileName);
}

bool fileExist(const char *fileName) {
    ifstream in(fileName, ios::in);
    
    if (in.fail()) {
        //cerr <<"\nError reading file: " << fileName <<"\nThe file doesn't exist!\n\n";
        in.close();
        return false;
    }
    
    in.close();
    return true;
}

int readInternalVars(const string &strVars, vector<string> &interVarsNames, vector<string> &interVarsVals){
    
    bool doneFind = false;
    istringstream issVars(strVars);
    while(!issVars.eof()) {
        string line;
        getline(issVars, line);
        if(line=="") continue;
        //if(line==LTA_GEN_DONE_MESSAGE){
        if(line.compare(LTA_GEN_DONE_MESSAGE)){
            doneFind = true;
            break;
        }
        
        
        istringstream issLine(line);
        string aux;
        std::vector<string> vAux;
        while (issLine >> aux){
            vAux.push_back(aux);
            cout << aux << endl;
        }
        if(vAux.size()==2){
            interVarsNames.push_back(vAux[0]);
            interVarsVals.push_back(vAux[1]);
        }
        else{
            cout << "line-----"<<line<<"-----"<< endl;
            cerr << "Error reading internal variables.\n";
            cerr << "Format is NOT two columns\n";
            cerr << "Entry: " << interVarsNames.size() << endl;
        }
    }
    
    
    if(doneFind == false){
        cerr << "Error reading internal variables.\n";
        cerr << "done flag not found\n";
    }
    
    return 0;
}

int create_image(vector< float> &pixels, const long nCols, const string outFString, const string &strVars="") {
    int naxis = 2;
    long naxes[2];
    naxes[0] = nCols;
    naxes[1] = (long) pixels.size() / naxes[0];
    cout << "output image size: " << naxes[0] << " cols x " << naxes[1] << "rows" << endl;
    
    int status = 0;
    fitsfile  *outfptr;
    fits_create_file(&outfptr, outFString.c_str(), &status);
    long totpix = naxes[0] * naxes[1];
    fits_create_img(outfptr, -32, naxis, naxes, &status);
    fits_write_img(outfptr, TFLOAT, 1, totpix, &(pixels[0]), &status);
    
    /* Write the sequencer variables in the FITS header */
    for (unsigned int i = 0; i < varName.size(); ++i)
    {
        const char *keyName = varName[i].c_str();
        char comment[FLEN_COMMENT] = "Sequencer variable";
        int datatype = TSTRING;
        char keyValue[FLEN_VALUE] = "value too long";
        if(varValue[i].size()<FLEN_VALUE) strcpy(keyValue, varValue[i].c_str());
        fits_update_key(outfptr, datatype, keyName, keyValue, comment, &status);
    }
    
    /* Write the LTA internal variables in the FITS header */
    // std::ifstream t("output_firmware_vars.txt");
    // std::string strVars((std::istreambuf_iterator<char>(t)),
    //                std::istreambuf_iterator<char>());
    vector<string> interVarsNames;
    vector<string> interVarsVals;
    int headerVarStatus = readInternalVars(strVars, interVarsNames, interVarsVals);
    if (headerVarStatus==0){
        for (int i = 0; i < interVarsNames.size(); ++i){
            const char *keyName = interVarsNames[i].c_str();
            char comment[FLEN_COMMENT] = "Internal variable";
            int datatype = TSTRING;
            char keyValue[FLEN_VALUE] = "value too long";
            if(interVarsVals[i].size()<FLEN_VALUE) strcpy(keyValue, interVarsVals[i].c_str());
            fits_update_key(outfptr, datatype, keyName, keyValue, comment, &status);
        }
    }
    
    
    fits_close_file(outfptr,  &status);
    return status;
}


int bin_to_fits(const vector <string> inFileName, const string outFileName, const long nCols, const string &strVars="") {
    
    //processing info
    processingInfo procInfo;
    // int returnCode = processCommandLineArgs( argc, argv, inFileName, outFileName, procInfo);
    // if(returnCode!=0){
    //     if(returnCode == 1) printCopyHelp(argv[0],true);
    //     if(returnCode == 2) printCopyHelp(argv[0]);
    //     return returnCode;
    // }
    
    //DUAL SLOPE INTEGRATOR CONFIGURATION OUTPUT
    cout << "number of columns = " << nCols << endl;
    
    ofstream myfile;
    TFile *fout;
    TTree *dataTree;
    dataVars dataEntry;
    if (textFileFlag) {
        //create text file
        stringstream outFileNameCsv;
        outFileNameCsv << outFileName.c_str() << ".csv";
        myfile.open(outFileNameCsv.str().c_str());
        //create root file
        stringstream outFileNameRoot;
        outFileNameRoot << outFileName.c_str() << ".root";
        fout = new TFile(outFileNameRoot.str().c_str(), "recreate");
        dataTree = new TTree("data", "data");
        //add Branches to tree
        dataTree->Branch("cntr", &(dataEntry.cntr), "cntr/S");
        dataTree->Branch("hdr", &(dataEntry.hdr), "hdr/S");
        dataTree->Branch("id", &(dataEntry.id), "id/S");
        dataTree->Branch("data", &(dataEntry.data), "data/I");
    }
    
    
    //aca empieza el loop para todos los archivos
    FILE *fin;
    
    //files to be processed
    cout << endl << "Processing files in the following order: " << endl;
    for (int i = 0; i < inFileName.size(); ++i) {
        cout << inFileName[i].c_str() << endl;
    }
    cout << endl;
    
    //image variables
    //ch1
    vector<vector <float>> pixelsVector;
    vector <int> channelIndex;
    int nChs = 0;
    int pixelCount = 0;
    
    long long dataCounter = 0;
    uint64_t dataIndexNew = 0;
    uint64_t dataIndexOld = 0;
    int packPreviousIndex = 0;
    int packCurrentIndex = 0;
    for (int u = 0; u < inFileName.size(); ++u) {//for all the binary files
        
        cout << "translating file: " << inFileName[u].c_str() << ", adc value up to here = " << dataCounter << endl;
        
        uint8_t header[3];
        uint8_t dataChar[8];
        uint8_t dataCharSorted[8];
        uint8_t wordCh1[4];
        uint8_t wordCh2[4];
        
        fin = fopen(inFileName[u].c_str(), "r");
        
        //declare auxiliary variables
        uint64_t *data;
        uint64_t * word64;
        int countLastBitPos = 63;
        int countFirstBitPos = 60;
        uint64_t counter64Mask = pow(2, countLastBitPos + 1) - pow(2, countFirstBitPos);
        int idLastBitPos = 59;
        int idFirstBitPos = 56;
        uint64_t id64Mask = pow(2, idLastBitPos + 1) - pow(2, idFirstBitPos);
        int dataLastBitPos = 31;
        int dataFirstBitPos = 0;
        uint64_t data64Mask = pow(2, dataLastBitPos + 1) - pow(2, dataFirstBitPos);
        //vars to convert negative value
        uint64_t positiveLimit = pow(2, dataLastBitPos);
        uint64_t twoToTheNBits = pow(2, dataLastBitPos + 1);
        
        
        //while, data in binary file
        uint8_t nData = 0;
        uint8_t packType = 0;
        uint8_t packCounter = 0;
        bool newFileFlag = true;
        while ( sizeof(header) == fread(header, 1, sizeof(header), fin) ) {//for all the data in the binary file, take the first 3 bytes of the data for each udp package
            
            nData = header[0];
            packType = header[1];
            packCounter = header[2];
            packCurrentIndex = packCounter;
            
            //check if we lost a udp package
            if (((packCurrentIndex - packPreviousIndex != 1) & (packPreviousIndex - packCurrentIndex != MAX_NUMBER_DATA_IN_UDP_PACK)) & dataCounter != 0) {
                cout << "something is wrong" << endl;
                cout << "udp package current index " << packCurrentIndex << endl;
                cout << "udp package previous index " << packPreviousIndex << endl;
                if (newFileFlag) {
                    cout << "First package of file" << endl;
                }
                cout << endl;
            }
            newFileFlag = false;
            
            if (gVerbosityTranslate) {
                cout << "nData " << (int) nData << endl;
                cout << "packType " << (int)packType << endl;
                cout << "packCounter " << (int)packCounter << endl;
            }
            //for, data in udp package
            for (uint8_t i = 0; i < nData; i++) {//for each 64-bit word in the udp package
                fread(dataChar, 1, sizeof(dataChar), fin);
                
                for (int j = 0; j < 8; j++) {
                    dataCharSorted[j] = dataChar[j];
                }
                
                //64-bit word from packer
                word64 = (uint64_t *)dataCharSorted;
                
                //extract information from the 64-bit word
                //counter
                uint64_t counterValue = *(word64)&counter64Mask;
                counterValue = counterValue >> countFirstBitPos;
                //channel ID
                uint64_t idValue = *(word64)&id64Mask;
                idValue = idValue >> idFirstBitPos;
                //data value
                uint64_t dataValue = *(word64)&data64Mask;
                int64_t dataValueSigned = (int64_t)dataValue;
                if (dataValueSigned > positiveLimit) dataValueSigned = dataValueSigned - twoToTheNBits;
                
                //Check data consistency
                dataIndexNew = counterValue;
                int maxCountValue = pow(2, countLastBitPos - countFirstBitPos + 1) - 1;
                if ((dataIndexNew - dataIndexOld != 1 & dataIndexNew - dataIndexOld != -maxCountValue) & dataCounter != 0) {
                    cout << endl;
                    cout << "we are loosing data" << endl;
                    cout << "current 64-bit-word counter = " << dataIndexNew << endl;
                    cout << "previous 64-bit-word counter = " << dataIndexOld << endl;
                    cout << "current adc data index " << dataCounter << endl;
                    cout << "current udp package index " << packCurrentIndex << endl;
                    cout << "previous udp package index " << packPreviousIndex << endl;
                    cout << "current package number adc data " << (int) nData << endl;
                    cout << "current package Type " << (int) packType << endl;
                    cout << endl;
                    
                    dataIndexOld = dataIndexNew;
                } else {
                    dataIndexOld = dataIndexNew;
                }
                
                
                //bit representation
                data = (uint64_t *)dataCharSorted;
                bitset <64> valueBin(*data);
                
                if (gVerbosityTranslate)
                {
                    cout << counterValue << " " << idValue << " " <<  dataValueSigned << " " << counterValue << " " << valueBin << endl;
                }
                if (textFileFlag && (dataCounter < procInfo.fileLength)) {
                    //file csv file
                    myfile << counterValue << "," << idValue << "," <<  dataValueSigned << "," << counterValue << "," << idValue << "," << dataValueSigned << "\n";
                    
                    //fill root file
                    dataEntry.cntr = counterValue;
                    dataEntry.hdr = 0;
                    dataEntry.id = idValue;
                    dataEntry.data = dataValueSigned;
                    dataTree->Fill();
                }
                dataCounter = dataCounter + 1;
                
                //SAVE PIXEL INFORMATION HERE
                if (idValue>=channelIndex.size()) {
                    while (channelIndex.size()<idValue) channelIndex.push_back(-1);
                    channelIndex.push_back(nChs);
                    pixelsVector.push_back(vector <float> ());
                    nChs ++;
                }
                pixelsVector[channelIndex[idValue]].push_back(dataValueSigned);
                
            }//for, data in udp package
            packPreviousIndex = packCurrentIndex;
        } //while, data in binary file
        
    } //for, binary files
    
    //aca cerrar loop de varios archivos
    
    if (textFileFlag) {
        myfile.close();
        dataTree->Write();
        fout->Close();
    }
    fclose(fin); // no longer needed
    
    //build the final images from the pixels vector subtracting the
    vector <float> gain1{0.5, 0.5, 0.4237, 0.7148};
    vector <float> gain2{0.5, 0.5, 0.4375, 0.6998};
    vector <float> gain3{0.4390, 0.2233, 0.5, 0.5};
    vector <float> gain4{0.5554, 0.2637, 0.5, 0.5};
    vector <vector <float>> gainMatrix;
    gainMatrix.push_back(gain1);
    gainMatrix.push_back(gain2);
    gainMatrix.push_back(gain3);
    gainMatrix.push_back(gain4);
   
    for (int i=0; i<pixelsVector.size(); i++) {

	cout << "el numero de pixeles en el canal " << i << " es " << pixelsVector[i].size() << endl;
    }
 
    int NUM_INTEG_PER_PIXEL = 3;
    vector<vector <float>> pixelFinValue(pixelsVector.size()*2, vector <float> ());;
    float newPedValue = 0;
    float newSigValue = 0;
    for (int imageInd = 0; imageInd < pixelsVector.size(); ++imageInd)
    {
        for (int pixelInd = 0; pixelInd<pixelsVector[0].size(); pixelInd = pixelInd + NUM_INTEG_PER_PIXEL) {
            
            if (imageInd == 0 || imageInd ==1) {
		// Original
                newPedValue = pixelsVector[imageInd][pixelInd+0] - gainMatrix[imageInd][2]*pixelsVector[2][pixelInd+0] - gainMatrix[imageInd][3]*pixelsVector[3][pixelInd+0];
                newSigValue = pixelsVector[imageInd][pixelInd+1] - gainMatrix[imageInd][2]*pixelsVector[2][pixelInd+1] - gainMatrix[imageInd][3]*pixelsVector[3][pixelInd+1];
                pixelFinValue[imageInd].push_back(-1.*(newSigValue - newPedValue));
		// Pixel comun
                newPedValue = pixelsVector[imageInd][pixelInd+0];
                newSigValue = pixelsVector[imageInd][pixelInd+1];

                pixelFinValue[imageInd+4].push_back(-1.*(newSigValue - newPedValue));
            } else if(imageInd == 2 || imageInd ==3) {
		// Original
                newPedValue = pixelsVector[imageInd][pixelInd+1] - gainMatrix[imageInd][0]*pixelsVector[0][pixelInd+1] - gainMatrix[imageInd][1]*pixelsVector[1][pixelInd+1];
                newSigValue = pixelsVector[imageInd][pixelInd+2] - gainMatrix[imageInd][0]*pixelsVector[0][pixelInd+2] - gainMatrix[imageInd][1]*pixelsVector[1][pixelInd+2];
              
		pixelFinValue[imageInd].push_back(-1.*(newSigValue - newPedValue)); 
		// Pixel comun
                newPedValue = pixelsVector[imageInd][pixelInd+1];
                newSigValue = pixelsVector[imageInd][pixelInd+2];

                pixelFinValue[imageInd+4].push_back(-1.*(newSigValue - newPedValue));
            }
        }
    }
    
    //check if fits files exist, if so delete them
    for (int imageInd = 0; imageInd < pixelFinValue.size(); ++imageInd)
    {
        stringstream outFString;
        outFString << outFileName << "_" << imageInd << ".fits";
        if (fileExist(outFString.str().c_str())) deleteFile(outFString.str().c_str());
        
        //create fits image
        int statusCreateImage = create_image(pixelFinValue[imageInd], nCols, outFString.str(), strVars);
        if (statusCreateImage != 0) {
            cerr << red << "something went wrong creating the image " << endl;
            return statusCreateImage;
        }
    }
    
    
    //    dataTree->Write();
    //   file->Close();
    //    tree->Write();
    //    byteTree->Write();
    //    decTree->Write();
    //    delete fout; // automatically deletes the "tree", too
    
    cout << "number of adc data = " << dataCounter << endl;
    cout << "number of pixels in channel 1 = " << pixelCount<< endl;
    return 0;
}
