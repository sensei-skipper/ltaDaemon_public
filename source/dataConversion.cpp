#include <vector>
#include <sstream>
#include <map>
#include <algorithm>
#include <unistd.h>
#include <libgen.h> //basename/dirname
#include <stdlib.h>

#include "fitsio.h"
#include "fitsio2.h"

#include "loguru.hpp" //logging

#ifdef USEROOT
#    include "TFile.h"
#    include "TTree.h"
#    include "TBranch.h"
#endif

#include "dataConversion.h"
#include "helperFunctions.h"

#define STARTUP_PACKETS 20

#define INTEGER_FITS true
//after writing an uncompressed .fits file, convert to compressed .fz
#define CONVERT_TO_COMPRESSED true
//write a compressed FITS file directly, instead of converting to compressed FITS in a second pass (this is slow, do not use)
#define COMPRESSED_FITS false

//number of pixels to accumulate in HDU buffers before writing to FITS
#define FITS_CHUNKSIZE 100000

using namespace std;

dataFileReader::dataFileReader(const vector<string> * inFileNames, int gVerbosityTranslate) {
    inFileNames_ = inFileNames;
    dataCount_ = 0;
    packetCount_ = 0;
    missingPackets = 0;
    dataGaps = 0;
    dataIndexOld = 0;
    packPreviousIndex = 0;
    iFile = 0;
    gVerbosityTranslate_ = gVerbosityTranslate;
    dataBuffer_.clear();

    textFileFlag = false;
    fileLength = 1;

    //files to be processed
    LOG_F(1, "Processing files in the following order: ");
    for (uint i = 0; i < inFileNames_->size(); ++i) {
        LOG_F(1, "%s", inFileNames_->at(i).c_str());
    }
    openFile(iFile);
}

dataFileReader::~dataFileReader() {
    if (textFileFlag) {
        myfile.close();
#ifdef USEROOT
        dataTree->Write();
        fout->Close();
#endif
    }
}

void dataFileReader::setupTextDump(const string outFileName) {
    textFileFlag = true;

    //create text file
    stringstream outFileNameCsv;
    outFileNameCsv << outFileName.c_str() << ".csv";
    myfile.open(outFileNameCsv.str().c_str());
    //create root file

#ifdef USEROOT
    stringstream outFileNameRoot;
    outFileNameRoot << outFileName.c_str() << ".root";
    fout = new TFile(outFileNameRoot.str().c_str(), "recreate");
    dataTree = new TTree("data", "data");
    //add Branches to tree
    dataTree->Branch("cntr", &(dataEntry.cntr), "cntr/S");
    dataTree->Branch("hdr", &(dataEntry.hdr), "hdr/S");
    dataTree->Branch("id", &(dataEntry.id), "id/S");
    dataTree->Branch("data", &(dataEntry.data), "data/I");
#endif
}

void dataFileReader::openFile(uint i) {
    LOG_F(INFO, "translating file: %s, adc count up to here = %lld", inFileNames_->at(i).c_str(), dataCount_);
    fin = fopen(inFileNames_->at(i).c_str(), "r");
}

bool dataFileReader::getWord(dataword_t &word64) {
    while (dataBuffer_.empty()) {
        //if we're here, we need to read more data for the buffer

        //declare auxiliary variables
        uint8_t header[3];
        dataword_t packetBuff[PACKETBUFF_SIZE];

        while ( sizeof(header) != fread(header, 1, sizeof(header), fin) ) {//for all the data in the binary file, take the first 3 bytes of the data for each udp package
                                                                           //if we're here, we failed to get a header out of the current file, and we need to open the next file
            fclose(fin);
            iFile++;
            if (iFile == inFileNames_->size()) {
                return false; //no more data files, therefore no more data
            } else {
                openFile(iFile);
            }
        }


        uint8_t nData = header[0];
        uint8_t packType = header[1];
        uint8_t packCounter = header[2];
        int packCurrentIndex = packCounter;
        packetCount_++;

        //check if we lost a udp packet
        if (((packCurrentIndex - packPreviousIndex + 256)%256) != 1 && dataCount_ != 0) {
            int logLevel = -1;//WARNING by default
                              //skipping numbers is normal in the first few packets, since the daemon is still trying to get the "Done" response to the sequencer start command
            if (packetCount_ < STARTUP_PACKETS) {
                logLevel = 1;
            } else {
                missingPackets++;
            }
            if (missingPackets>10) logLevel = 1;
            VLOG_F(logLevel, "lost packet? current packet index %d, previous packet index %d, packetCount %lld, dataCount %lld", packCurrentIndex, packPreviousIndex, packetCount_, dataCount_);
            if (missingPackets==10) LOG_F(WARNING, "more than 10 mismatched packet indices, silencing further messages");
        }

        if (gVerbosityTranslate_) {
            LOG_F(INFO, "nData %d packType %d packCounter %d", nData, packType, packCounter);
        }

        fread(&packetBuff, sizeof(uint64_t), nData, fin);
        //for, data in udp package
        for (uint8_t i = 0; i < nData; i++) {//for each 64-bit word in the udp package
                                             //64-bit word from packer
            word64 = packetBuff[i];

            //extract information from the 64-bit word
            uint8_t counterValue = word64.counter;

            //Check data consistency
            int dataIndexNew = counterValue;
            if (((dataIndexNew - dataIndexOld + 16)%16) != 1 && dataCount_ != 0) {
                LOG_F(WARNING, "lost data: current word index %d (prev %d), current packet index %d (prev %d), this is word %d of the packet and word %lld of the run", dataIndexNew, dataIndexOld, packCurrentIndex, packPreviousIndex, i, dataCount_);
                dataGaps++;
            }

            if (gVerbosityTranslate_ || textFileFlag) {
                uint8_t idValue = word64.chID;
                int32_t dataValueSigned = word64.cdsSamp;
                if (gVerbosityTranslate_)
                {
                    LOG_F(INFO, "%d %d %lld %d %llu", counterValue, idValue, dataValueSigned, counterValue, word64);
                }

                if (textFileFlag && (dataCount_ < fileLength)) {
                    //file csv file
                    myfile << counterValue << "," << idValue << "," <<  dataValueSigned << "," << counterValue << "," << idValue << "," << dataValueSigned << "\n";

                    //fill root file
#ifdef USEROOT
                    dataEntry.cntr = counterValue;
                    dataEntry.hdr = 0;
                    dataEntry.id = idValue;
                    dataEntry.data = dataValueSigned;
                    dataTree->Fill();
#endif 

                }
            }

            dataIndexOld = dataIndexNew;

            dataBuffer_.push_back(word64);
            dataCount_++;
        }//for, data in udp package
        packPreviousIndex = packCurrentIndex;
    }

    //now we have data in the buffer; take data from the buffer
    word64 = dataBuffer_.front();
    dataBuffer_.pop_front();
    return true;
}

void dataFileReader::deleteDatFiles() {
    LOG_F(INFO, "deleting .dat files");
    for (uint iFile =0; iFile<inFileNames_->size(); iFile++) {
        deleteFile(inFileNames_->at(iFile).c_str());
    }
}

int mean(const vector <double> data, double &result)
{
    //calculate mean
    double val = 0;
    for (uint i = 0; i<data.size(); i++) {
        val+=data[i];
    }
    result = val/((double) data.size());

    return 0;
}

int covariance(const vector <double> data1, const vector <double> data2, double &result)
{
    //calculate mean
    double mu1 = 0;
    mean(data1, mu1);
    double mu2 = 0;
    mean(data2, mu2);
    //calculate covariance
    double val = 0;
    for (uint i = 0; i<data1.size(); i++) {
        val+=(data1[i]-mu1)*(data2[i]-mu2);
    }
    result = val/((double) data1.size()-1.);

    return 0;
}

FitsWriter::FitsWriter(const int nHdu, const long nCols, const long nRows, const string outFString, const bool isInteger, const bool isCompressed)
{
    nHdu_ = nHdu;
    nCols_ = nCols;
    nRows_ = nRows;
    isInteger_ = isInteger;
    pixExpected = nCols*nRows;
    isCompressed_ = isCompressed;
    outFString_ = outFString;
    fileOpen = true;

    LOG_F(INFO,"creating empty image file %s with %d HDUs, %ld cols, %ld rows", outFString.c_str(), nHdu, nCols, nRows);

    int status = 0;

    if (fileExist(outFString.c_str())) {
        LOG_F(WARNING, "Will overwrite: %s", outFString.c_str());
        deleteFile(outFString.c_str());
    }
    if (isCompressed_) {
        fits_create_file(&outfptr_, (outFString+"[compress]").c_str(), &status);
    } else {
        fits_create_file(&outfptr_, outFString.c_str(), &status);
    }

    LOG_IF_F(WARNING, status!=0, "CFITSIO error: %d", status);
}

FitsWriter::~FitsWriter()
{
    close(); //close the FITS file, if not closed yet
}

void FitsWriter::add_extra_var(const string extraVar)
{
    extraVars_.push_back(extraVar);
}

int FitsWriter::create_img(const vector<imageVar> vars, bool writeTimestamps)
{
    int status = 0;
    writeTimestamps_ = writeTimestamps;

    int naxis = 2;
    long naxes[2];
    naxes[0] = nCols_;
    naxes[1] = nRows_;

    long totpix = naxes[0] * naxes[1];
    for (int hduInd = 0; hduInd< nHdu_; hduInd++) {
        //LONG_IMG: signed 32-bit integer
        //FLOAT_IMG: signed 32-bit float
        fits_create_img(outfptr_, isInteger_?LONG_IMG:FLOAT_IMG, naxis, naxes, &status);
        LOG_IF_F(WARNING, status!=0, "CFITSIO error: %d", status);
        pixelCounts[hduInd+1] = 0;

        write_vars(vars);
        for (unsigned int i=0; i<extraVars_.size(); ++i)
        {
            fits_write_key_null(outfptr_, extraVars_[i].c_str(), NULL, &status);
        }
    }

    // CFITSIO does not allocate the file immediately, it buffers stuff in memory and writes to file as needed
    // so the first time we write a bunch of pixels to the later HDUs, CFITSIO will suddenly need to allocate a lot of disk
    // we would prefer to get the allocation done at initialization, and avoid latency during data processing
    // to make this happen, we write a block of zeroes to the end of the last HDU
    // MINDIRECT is the minimum number of bytes to force CFITSIO to write to disk instead of buffering
    // this forces CFITSIO to immediately allocate (write zeroes to) the full extent of the image
    //
    // there might be more efficient ways to do this allocation using low-level file ops (e.g. ftruncate/fallocate)
    // but we'd probably need to monkey with CFITSIO itself to make those work
    int nzeros = MINDIRECT / (isInteger_?sizeof(int):sizeof(float));
    if (nzeros <= totpix) {
        long fpixel[2];
        fpixel[0] = ((totpix - nzeros) % nCols_) + 1;
        fpixel[1] = ((totpix - nzeros) / nCols_) + 1;
        int hduType = IMAGE_HDU;
        fits_movabs_hdu(outfptr_, isCompressed_?nHdu_+1:nHdu_, &hduType, &status);
        if (isInteger_) {
            int zeros[nzeros]; // initialized to zero
            fits_write_pix(outfptr_, TINT, fpixel, nzeros, &(zeros[0]), &status);
        } else {
            float zeros[nzeros]; // initialized to zero
            fits_write_pix(outfptr_, TFLOAT, fpixel, nzeros, &(zeros[0]), &status);
        }
    }

    return status;
}

int FitsWriter::close()
{
    int status = 0;
    if (fileOpen) {
        fits_close_file(outfptr_,  &status);
        LOG_F(INFO, "closing file");
        LOG_IF_F(WARNING, status!=0, "CFITSIO error: %d", status);
        fileOpen = false;
    }
    return status;
}

//Writes header variables and DATE to the specified HDU, or the current HDU (if hdunum=-1).
//For best performance, placeholder values should already have been written (using add_extra_var) for all variables written here - otherwise you may waste a lot of time expanding the FITS header to accomodate new variables.
int FitsWriter::write_vars(const vector<imageVar> vars, int hdunum)
{
    LOG_F(1, "hdunum %d: writing %d vars", hdunum, vars.size());
    int status = 0;
    int hduType = IMAGE_HDU;
    if (hdunum>=0) fits_movabs_hdu(outfptr_, isCompressed_?hdunum+1:hdunum, &hduType, &status);
    for (unsigned int i = 0; i < vars.size(); ++i)
    {
        const char *keyName = vars[i].name.c_str();
        if (vars[i].value.size()==0) {
            fits_write_key_null(outfptr_, keyName, NULL, &status);
            continue;
        }
        fits_update_key_longstr(outfptr_, keyName, vars[i].value.c_str(), vars[i].comment.c_str(), &status);
        LOG_IF_F(WARNING, status!=0, "CFITSIO error: %d", status);
    }
    if (writeTimestamps_) {
        fits_write_date(outfptr_, &status);
        LOG_IF_F(WARNING, status!=0, "CFITSIO error: %d", status);
    }
    return status;
}

// returns true if pixel count matches expected count
bool FitsWriter::save_counts(int hdunum, int chID, int runNum)
{
    vector<imageVar> vars;
    vars.push_back({"chID", to_string(chID), "amplifier ID for this HDU"});
    vars.push_back({"RUNID", to_string(runNum), "run index"});
    vars.push_back({"NPIX", to_string(getPixelCount(hdunum)), "number of pixels read from LTA"});
    write_vars(vars, hdunum);

    LOG_F(INFO, "hdunum %d: chID %d, wrote %ld pixels to FITS", hdunum, chID, getPixelCount(hdunum));
    if (getPixelCount(hdunum) != getPixExpected()) {
        LOG_F(WARNING, "wrote %ld pixels, expected %ld!", getPixelCount(hdunum), getPixExpected());
        return false;
    }
    return true;
}

template <typename T> int FitsWriter::dump_to_fits(int hdunum, long * fpixel, vector<T> &pixels, int npix)
{
    int hduType = IMAGE_HDU;
    int status = 0;
    fits_movabs_hdu(outfptr_, isCompressed_?hdunum+1:hdunum, &hduType, &status);
    LOG_IF_F(WARNING, status!=0, "CFITSIO error: %d", status);
    //TINT: native int (could be int32 or int64)
    //TFLOAT: native float (could be 32-bit or 64-bit float)

    long pixToWrite = npix;
    if (pixToWrite==-1) {//default: write all pixels
        pixToWrite = pixels.size();
    }
    fits_write_pix(outfptr_, isInteger_?TINT:TFLOAT, fpixel, pixToWrite, &(pixels[0]), &status);
    LOG_IF_F(WARNING, status!=0, "CFITSIO error: %d", status);

    return 0;
}

template <typename T> int FitsWriter::dump_to_fits(int hdunum, vector<T> &pixels, bool clearVector, int npix)
{
    if (pixels.empty()) {//if we're asked to write 0 pixels, do nothing
        return 0;
    }
    int pixRow = (pixelCounts[hdunum] / nCols_) + 1;
    int pixCol = (pixelCounts[hdunum] % nCols_) + 1;
    long fpixel[2] = {pixCol, pixRow};
    //LOG_F(1,"hdunum %d: writing %d pixels starting at (%d, %d)", hdunum, pixels.size(), fpixel[0], fpixel[1]);

    long spaceRemaining = pixExpected - pixelCounts[hdunum];
    pixelCounts[hdunum] += pixels.size();

    if (spaceRemaining <=0) {
        LOG_F(WARNING, "too much data for hdunum %d, discarding %ld excess samples", hdunum, pixels.size());
        pixels.clear(); //delete all the data
        return 0;
    } else if ((long) pixels.size() > spaceRemaining) {
        LOG_F(WARNING, "too much data for hdunum %d, discarding %ld excess samples", hdunum, pixels.size()-spaceRemaining);
        pixels.erase(pixels.begin()+spaceRemaining, pixels.end()); //delete the data that will not fit in the image
    }

    int status = dump_to_fits(hdunum, fpixel, pixels, npix);
    if (clearVector) {
        if (npix==-1) {
            pixels.clear();
        } else {
            pixels.erase(pixels.begin(), pixels.begin()+npix);
        }
    }
    return status;
}

int compressFits(const char* infits, const char* outfits)
{
    // adapted from CFITSIO fpack
    int status = 0;
    fitsfile* infptr;
    fitsfile* outfptr;
    bool do_checksums = false;

    fits_open_file (&infptr, infits, READONLY, &status);

    fits_create_file (&outfptr, outfits, &status);

    while (!status) {
        /*  LOOP OVER EACH HDU */
        long	naxes[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
        int totpix=0, naxis=0, ii, hdutype, bitpix;

        fits_set_lossy_int (outfptr, 0, &status);
        fits_set_compression_type (outfptr, RICE_1, &status);
        long ntile[MAX_COMPRESS_DIM];
        ntile[0] = -1;
        for (ii=1; ii < MAX_COMPRESS_DIM; ii++) {
            ntile[ii] = 1;
        }

        fits_set_tile_dim (outfptr, 6, ntile, &status);
        fits_set_quantize_method(outfptr, 1, &status);
        fits_set_quantize_level (outfptr, 4., &status);
        fits_set_dither_offset(outfptr, 0, &status);
        fits_set_hcomp_scale (outfptr, 0., &status);
        fits_set_hcomp_smooth (outfptr, 0, &status);

        fits_get_hdu_type (infptr, &hdutype, &status);

        if (hdutype != IMAGE_HDU) {
            LOG_F(ERROR, "expected an image HDU to compress, got something else");
        } else {  /* remaining code deals only with IMAGE HDUs */
            fits_get_img_param (infptr, 9, &bitpix, &naxis, naxes, &status);
            for (totpix=1, ii=0; ii < 9; ii++) totpix *= naxes[ii];

            /* finally, do the actual image compression */
            fits_img_compress (infptr, outfptr, &status);
        }

        if (do_checksums) {
            fits_write_chksum (outfptr, &status);
        }

        fits_movrel_hdu (infptr, 1, NULL, &status);
    }

    if (status == END_OF_FILE) status = 0;

    /* set checksum for case of newly created primary HDU	 */

    if (do_checksums) {
        fits_movabs_hdu (outfptr, 1, NULL, &status);
        fits_write_chksum (outfptr, &status);
    }

    fits_close_file (outfptr, &status);
    fits_close_file (infptr, &status);
    return status;
}

DataConversion::DataConversion() {
    textFileFlag = 0;
    gVerbosityTranslate = false;
    //gain matrix for interleaving
    gainMatrix = {vector<double>(2,-0.5), vector<double>(2,-0.5), vector<double>(2,-0.5), vector<double>(2,-0.5)};
    writeTimestamps_ = 1;
    deleteDats_ = 1;
    keepTemp_ = 0;
}

int DataConversion::bin_to_fits_interleaving(const string outFileName, const uint nCols, const uint nRows, const vector<imageVar> vars) {

    //create output image
    stringstream outFString;
    outFString << outFileName << ".fits";
    lastFilename_ = outFString.str();

    FitsWriter outf(LTA_NUMBER_OF_CHANNELS, nCols, nRows, outFString.str(), false, false);
    outf.create_img(vars);

    //image variables
    //ch1
    vector<vector <float>> pixelsVector;
    vector <int> channelIndex;
    int nChs = 0;

    dataFileReader reader(inFileNames_, gVerbosityTranslate);

    if (textFileFlag) reader.setupTextDump(outFileName);

    ////64-bit word from packer
    dataword_t word64;
    while (reader.getWord(word64)) {
        uint8_t idValue = word64.chID;
        int32_t dataValueSigned = word64.cdsSamp;

        //SAVE PIXEL INFORMATION HERE
        if (idValue>=channelIndex.size()) {
            while (channelIndex.size()<idValue) channelIndex.push_back(-1);
            channelIndex.push_back(nChs);
            pixelsVector.push_back(vector <float> ());
            nChs ++;
        }
        pixelsVector[channelIndex[idValue]].push_back(dataValueSigned);

        //clear pixelsVector vector

        //if number of pixeles available is equal to the column number I dump them to disc to avoid RAM saturation
        if (pixelsVector[0].size()>nCols) {
            compute_pixel_value(outf, pixelsVector);
        }

    } //while

    //save the last pixels in the files
    compute_pixel_value(outf, pixelsVector);

    //output image is closed when FitsWriter goes out of scope
    return 0;
}

int DataConversion::compute_pixel_value(FitsWriter &outf, vector<vector <float>> &pixelsVector)
{
    //get the final pixel value
    vector<vector <float>> pixelFinInterleaving(pixelsVector.size(), vector <float> ());
    vector<vector <float>> pixelFinStandard(pixelsVector.size(), vector <float> ());
    float newPedValue = 0;
    float newSigValue = 0;
    for (uint imageInd = 0; imageInd < pixelsVector.size(); ++imageInd)
    {

        for (uint pixelInd = 0; pixelInd<pixelsVector[imageInd].size()-INTERLEAVING_NUM_INTEG_PER_PIXEL + 1; pixelInd = pixelInd + INTERLEAVING_NUM_INTEG_PER_PIXEL) {


            if (imageInd == 0 || imageInd ==1) {
                // Original
                newPedValue = pixelsVector[imageInd][pixelInd+0] + gainMatrix[imageInd][0]*pixelsVector[2][pixelInd+0] + gainMatrix[imageInd][1]*pixelsVector[3][pixelInd+0];
                newSigValue = pixelsVector[imageInd][pixelInd+1] + gainMatrix[imageInd][0]*pixelsVector[2][pixelInd+1] + gainMatrix[imageInd][1]*pixelsVector[3][pixelInd+1];
                pixelFinInterleaving[imageInd].push_back(-1.*(newSigValue - newPedValue));
                // Pixel comun
                newPedValue = pixelsVector[imageInd][pixelInd+0];
                newSigValue = pixelsVector[imageInd][pixelInd+1];

                pixelFinStandard[imageInd].push_back(-1.*(newSigValue - newPedValue));
            } else if(imageInd == 2 || imageInd ==3) {
                // Original
                newPedValue = pixelsVector[imageInd][pixelInd+1] + gainMatrix[imageInd][0]*pixelsVector[0][pixelInd+1] + gainMatrix[imageInd][1]*pixelsVector[1][pixelInd+1];
                newSigValue = pixelsVector[imageInd][pixelInd+2] + gainMatrix[imageInd][0]*pixelsVector[0][pixelInd+2] + gainMatrix[imageInd][1]*pixelsVector[1][pixelInd+2];

                pixelFinInterleaving[imageInd].push_back(-1.*(newSigValue - newPedValue));
                // Pixel comun
                newPedValue = pixelsVector[imageInd][pixelInd+1];
                newSigValue = pixelsVector[imageInd][pixelInd+2];

                pixelFinStandard[imageInd].push_back(-1.*(newSigValue - newPedValue));
            }
        }
    }
    //mando las cosas a disco
    for (uint hduInd = 0; hduInd<pixelFinInterleaving.size(); hduInd++) {
        outf.dump_to_fits(hduInd + 1, pixelFinInterleaving[hduInd], false);
    }

    //clear pixelsVector but keep not used information
    for (uint vecInd = 0; vecInd<pixelsVector.size(); vecInd++) {
        //pixelsVector[vecInd].clear();//hacer un clear controlado
        // erase the first 3 elements:

        long nInfoUsed = pixelFinInterleaving[vecInd].size()*INTERLEAVING_NUM_INTEG_PER_PIXEL;
        pixelsVector[vecInd].erase (pixelsVector[vecInd].begin(),pixelsVector[vecInd].begin()+nInfoUsed);
    }

    return 0;
}

int DataConversion::bin_to_fits(const string outFileName, const string tempFileName, const uint nCols, const uint nRows, const uint runNum, const vector<imageVar> vars) {

    LOG_F(INFO, "number of columns = %d", nCols);

    //const uint chunkSize = ((FITS_CHUNKSIZE/nCols)+1) * nCols;
    const uint chunkSize = FITS_CHUNKSIZE;
    //const uint chunkSize = nCols;
    LOG_F(1, "writing %d pixels at a time", chunkSize);

    //initialize output fits file
    string outFString;
    if (CONVERT_TO_COMPRESSED) {
        outFString = tempFileName+".fits";
    } else {
        outFString = outFileName+".fits";
    }
    lastFilename_ = outFString;

    FitsWriter outf(LTA_NUMBER_OF_CHANNELS, nCols, nRows, outFString, INTEGER_FITS, COMPRESSED_FITS);

    outf.create_img(vars, writeTimestamps_);
    LOG_F(INFO, "initialized FITS file");

    //image variables
#if INTEGER_FITS
    map<int, vector<int> > pixelsVectorMap; // Maps CHs to vectors with the pixels values
#else
    map<int, vector<float> > pixelsVectorMap; // Maps CHs to vectors with the pixels values
#endif

    dataFileReader reader(inFileNames_, gVerbosityTranslate);

    if (textFileFlag) reader.setupTextDump(outFileName);

    ////64-bit word from packer
    dataword_t word64;
    // number of words buffered in pixelsVectorMap
    int bufferedWords = 0;
    bool moreData;
    do {
        moreData = reader.getWord(word64);
        if (moreData) {
            uint8_t idValue = word64.chID;
            int32_t dataValueSigned = word64.cdsSamp;

            //SAVE PIXEL INFORMATION HERE
            pixelsVectorMap[idValue].push_back(-1 * dataValueSigned);
            bufferedWords++;
        }

        if (!moreData || bufferedWords >= chunkSize) {
            if (pixelsVectorMap.size()==LTA_NUMBER_OF_CHANNELS) {//we've seen all channels, so it's safe to assign the channels to HDUs
                int hdunum = 1;
                for ( auto chPixIt = pixelsVectorMap.begin(); chPixIt != pixelsVectorMap.end(); ++chPixIt ){
                    //C++ maps are sorted, so the HDUs are automatically written in channel order
                    outf.dump_to_fits(hdunum, chPixIt->second, true, -1);
                    hdunum++;
                }
                bufferedWords = 0;
            } else if (pixelsVectorMap.size()>LTA_NUMBER_OF_CHANNELS) {
                LOG_F(ERROR, "too many channels! %ld channels where %d were expected:", pixelsVectorMap.size(), LTA_NUMBER_OF_CHANNELS);
                for ( auto chPixIt = pixelsVectorMap.begin(); chPixIt != pixelsVectorMap.end(); ++chPixIt ){
                    LOG_F(ERROR, "chID %d", chPixIt->first);
                }
                LOG_F(ERROR, "stop processing this image");
                return 1;
            }
        }
    } while (moreData);

    //aca cerrar loop de varios archivos

    int hdunum = 1;
    bool dataGood = true;
    bool noData = true; //true if all HDUs are totally empty
    if (reader.getMissingPackets() != 0) dataGood = false;
    if (reader.getDataGaps() != 0) dataGood = false;
    if (pixelsVectorMap.size() != LTA_NUMBER_OF_CHANNELS) dataGood = false;
    for( auto chPixIt = pixelsVectorMap.begin(); chPixIt != pixelsVectorMap.end(); ++chPixIt ){
        dataGood &= outf.save_counts(hdunum, chPixIt->first, runNum);
        hdunum++;
    }

    outf.close(); //close output FITS file
    LOG_IF_F(INFO, dataGood, "data good: no missing data and all HDUs got the correct number of pixels");

    if (dataGood) {
        // keep FITS file, delete DAT files if deleteDats
        if (deleteDats_ || !keepTemp_) reader.deleteDatFiles();
    } else if (reader.getDataCount()==0) {
        // we didn't get any data from the LTA - delete FITS and DATs
        reader.deleteDatFiles();
        deleteFile(outFString.c_str());
    } else {
        if (keepTemp_) {
            // we got some data but not all - make symlinks to the temp files in case we want to inspect them
            if (outFileName.compare(tempFileName)!=0) {
                //get the output dir
                char *dummy2 = strdup(outFileName.c_str());
                char *outDir = dirname(dummy2);
                for (uint i=0; i<inFileNames_->size(); i++) {
                    //convert dat file path to absolute
                    char filepathbuffer[PATH_MAX+1];
                    char* abspath = realpath(inFileNames_->at(i).c_str(), filepathbuffer);

                    //symlink goes in the output dir, and has the same name as the original file
                    char *dummy1 = strdup(inFileNames_->at(i).c_str());
                    char *outBasename = basename(dummy1);
                    char linkpathbuffer[PATH_MAX+1];
                    sprintf(linkpathbuffer, "%s/%s", outDir, outBasename);
                    symlink(abspath, linkpathbuffer);
                    free(dummy1);
                }
                free(dummy2);
            }
        } else {
            reader.deleteDatFiles();
        }
    }
    LOG_F(INFO,"wrote uncompressed image");

    if (CONVERT_TO_COMPRESSED && reader.getDataCount()!=0) {
        string compFString = outFileName+".fz";
        lastFilename_ = compFString;
        if (fileExist(compFString.c_str())) {
            LOG_F(WARNING, "Will overwrite: %s", compFString.c_str());
            deleteFile(compFString.c_str());
        }
        int status = compressFits(outFString.c_str(), compFString.c_str());
        LOG_IF_F(WARNING, status!=0, "CFITSIO error: %d", status);
        LOG_F(INFO,"wrote compressed image");
        if ((dataGood && status==0) || !keepTemp_) {
            LOG_F(INFO,"deleting uncompressed image: %s", outFString.c_str());
            deleteFile(outFString.c_str());
        } else {
            if (outFileName.compare(tempFileName)!=0) {
                //convert outFString (typically a relative path) to an absolute path
                char pathbuffer[PATH_MAX+1];
                char* abspath = realpath(outFString.c_str(), pathbuffer);
                //make a symlink in the output directory pointing to the temporary file
                symlink(abspath, (outFileName+".fits").c_str());
            }
        }
    }

    LOG_F(INFO, "image done! number of adc data = %lld", reader.getDataCount());
    return 0;
}

template <typename T> int DataConversion::compute_pixel_value_smart(FitsWriter &outfAll, FitsWriter &outfAve, const smartImageDimensions_t dims, const vector<vector <T>> &pixelsVector, vector <uint> &rowInd, vector <uint> &colInd, vector<vector <T>> &fRowImageAll)
{
    //ask how big is the output image with all samples
    long outImageColSize = dims.nColsAllMax;
    long outImageRowSize = dims.nRowsAll;

    //ask how big is the output image with averaged samples
    long outImageAveColSize = dims.nColsAve;

    for (uint hduInd = 0; hduInd<pixelsVector.size(); hduInd++) {
        int hdunum = hduInd + 1;
        uint usedPixInd = 0;
        while (usedPixInd<pixelsVector[hduInd].size()) {
            //check if there is more data than expected
            if (rowInd[hduInd] >=outImageRowSize){
                LOG_F(WARNING, "===========================================");
                LOG_F(WARNING, "GETTING MORE DATA FROM THE BOARD THAN EXPECTED");
                LOG_F(WARNING, "IGNORING EXTRA DATA");
                LOG_F(WARNING, "===========================================");
                break; 
            } 

            int pixelsNeededInRow = dims.nCols[rowInd[hduInd]]-colInd[hduInd];
            int pixelsAvailable = pixelsVector[hduInd].size()-usedPixInd;
            if (pixelsAvailable < pixelsNeededInRow) {//just copy everything into the row buffer
                for (int i=0; i<pixelsAvailable; i++){
                    fRowImageAll[hduInd][colInd[hduInd]+i] = pixelsVector[hduInd][usedPixInd+i];
                }
                colInd[hduInd]+=pixelsAvailable;
                usedPixInd+=pixelsAvailable;
            } else {//fill the row buffer and write to FITS
                    //fill the temporary vector of the Row of the image with all the samples
                for (int i=0; i<pixelsNeededInRow; i++){
                    fRowImageAll[hduInd][colInd[hduInd]+i] = pixelsVector[hduInd][usedPixInd+i];
                }
                colInd[hduInd]+=pixelsNeededInRow;
                usedPixInd+=pixelsNeededInRow;

                //complete with not real pixels
                int nNotReal = outImageColSize - dims.nCols[rowInd[hduInd]];
                if (nNotReal > 0) {//try to put fake pixels to complete the output image
                    for (int i=0; i<nNotReal; i++) {
                        fRowImageAll[hduInd][colInd[hduInd]+i] = SMART_FLAG_NOT_REAL_PIXELS;
                    }
                    colInd[hduInd]+=nNotReal;
                }
                outfAll.dump_to_fits(hdunum, fRowImageAll[hduInd], false); //don't clear the row vector

                //average data
                int sampInd = 0;
                vector <float> fRowImageAve(outImageAveColSize, 0);
                for (uint iCol=0; iCol< dims.nSampsPixRow[rowInd[hduInd]].size(); iCol++){
                    int nSamp = dims.nSampsPixRow[rowInd[hduInd]][iCol];
                    for (int j=0; j<nSamp; j++) {
                        fRowImageAve[iCol] += fRowImageAll[hduInd][sampInd];
                        sampInd++;
                    }
                    fRowImageAve[iCol]=fRowImageAve[iCol]/nSamp;
                }
                //dump averaged data to disc
                outfAve.dump_to_fits(hdunum, fRowImageAve);

                rowInd[hduInd] ++;
                if (rowInd[hduInd]<dims.nCols.size()) {//check there is no extra row to add
                    for (uint u=0;u<fRowImageAll[hduInd].size();u++) fRowImageAll[hduInd][u] = -7777;//"clean" values, so if there is a bug is easier to find
                    colInd[hduInd] = 0;
                }
            }
        }
    }

    //FROM HERE, ALL THE INFORMATION FROM pixelsVector HAS BEEN USED

    return 0;
}

int DataConversion::bin_to_fits_smart(const string outFileName, const smartImageDimensions_t dims, const vector<imageVar> vars) {

    //create index and auxiliary containers needed to save data in the output file
    vector <uint> rowInd(LTA_NUMBER_OF_CHANNELS,0);
    vector <uint> colInd(LTA_NUMBER_OF_CHANNELS,0);
#if INTEGER_FITS
    vector <vector<int>> fRowImageAll(LTA_NUMBER_OF_CHANNELS,vector<int>(dims.nColsAllMax,-7777));//any number
#else
    vector <vector<float>> fRowImageAll(LTA_NUMBER_OF_CHANNELS,vector<float>(dims.nColsAllMax,-7777));//any number
#endif

    //create output image
    string outFAveString = outFileName+"_ave.fits";
    string outFAllString = outFileName+"_all.fits";
    lastFilename_ = outFAllString;

    LOG_F(INFO, "TAMANO DE LA IMAGEN FINAL %d %d", dims.nRowsAll, dims.nColsAllMax);
    vector <unsigned long> totPixInter(LTA_NUMBER_OF_CHANNELS,0); //counter of the number of pixels already processed
    FitsWriter outfAve(LTA_NUMBER_OF_CHANNELS, dims.nColsAve, dims.nRowsAve, outFAveString, false, false);
    FitsWriter outfAll(LTA_NUMBER_OF_CHANNELS, dims.nColsAllMax, dims.nRowsAll, outFAllString, INTEGER_FITS, COMPRESSED_FITS);
    outfAve.create_img(vars);
    outfAll.create_img(vars);

    //image variables
#if INTEGER_FITS
    vector<vector <int>> pixelsVector;
#else
    vector<vector <float>> pixelsVector;
#endif
    vector <int> channelIndex;
    int nChs = 0;

    long long dataCounter = 0;

    dataFileReader reader(inFileNames_, gVerbosityTranslate);

    if (textFileFlag) reader.setupTextDump(outFileName);

    ////64-bit word from packer
    dataword_t word64;
    while (reader.getWord(word64)) {
        uint8_t idValue = word64.chID;
        int32_t dataValueSigned = word64.cdsSamp;

        dataCounter = dataCounter + 1;

        //SAVE PIXEL INFORMATION HERE
        if (idValue>=channelIndex.size()) {
            while (channelIndex.size()<idValue) channelIndex.push_back(-1);
            channelIndex.push_back(nChs);
#if INTEGER_FITS
            pixelsVector.push_back(vector <int> ());
#else
            pixelsVector.push_back(vector <float> ());
#endif
            nChs ++;
        }
        pixelsVector[channelIndex[idValue]].push_back(-1*dataValueSigned);

        //if number of pixeles available is equal to the column number I dump them to disc to avoid RAM saturation
        if (pixelsVector[0].size()>dims.nColsAllMax) {
            compute_pixel_value_smart(outfAll, outfAve, dims, pixelsVector, rowInd, colInd, fRowImageAll);
            //clear pixelsVector vector
            for (uint vecInd = 0; vecInd<pixelsVector.size(); vecInd++) pixelsVector[vecInd].clear();
        }
    } //while

    //save the last pixels in the files
    compute_pixel_value_smart(outfAll, outfAve, dims, pixelsVector, rowInd, colInd, fRowImageAll);
    for (uint vecInd = 0; vecInd<pixelsVector.size(); vecInd++) pixelsVector[vecInd].clear();

    LOG_F(INFO, "number of adc data = %lld", dataCounter);
    //LOG_F(INFO, "number of pixels in channel 1 = %d", pixelCount);

    outfAll.close(); //close output FITS file

    if (CONVERT_TO_COMPRESSED) {
        string compFAllString = outFileName+"_all.fz";
        lastFilename_ = compFAllString;
        if (fileExist(compFAllString.c_str())) {
            LOG_F(WARNING, "Will overwrite: %s", compFAllString.c_str());
            deleteFile(compFAllString.c_str());
        }

        int status = compressFits(outFAllString.c_str(), compFAllString.c_str());
        LOG_IF_F(WARNING, status!=0, "CFITSIO error: %d", status);
        if (status==0) {
            LOG_F(INFO,"deleting uncompressed image: %s", outFAllString.c_str());
            deleteFile(outFAllString.c_str());
        }
    }
    //output image is closed when FitsWriter goes out of scope
    return 0;
}

int DataConversion::bin_to_raw(const string outFileName) {
    //output files
    ofstream myfile;

#ifdef USEROOT
    TFile *fout;
    TTree *dataTree;
    dataVars dataEntry;
#endif

    //create text file
    stringstream outFileNameCsv;
    outFileNameCsv << outFileName.c_str() << ".csv";
    myfile.open(outFileNameCsv.str().c_str());
    //create root file
    stringstream outFileNameRoot;
    outFileNameRoot << outFileName.c_str() << ".root";

#ifdef USEROOT
    fout = new TFile(outFileNameRoot.str().c_str(), "recreate");
    lastFilename_ = outFileNameRoot.str();

    dataTree = new TTree("data", "data");
    //add Branches to tree
    dataTree->Branch("cntr", &(dataEntry.cntr), "cntr/S");
    dataTree->Branch("hdr", &(dataEntry.hdr), "hdr/S");
    dataTree->Branch("id", &(dataEntry.id), "id/S");
    dataTree->Branch("data", &(dataEntry.data), "data/I");
#endif

    dataFileReader reader(inFileNames_, gVerbosityTranslate);

    ////64-bit word from packer
    dataword_t word64;
    while (reader.getWord(word64)) {
        uint32_t idValue = word64.chID;
        uint32_t counterValue = word64.counter;
        //int64_t dataValueSigned = dataFileReader::decodeDataValueSigned(word64);

        //extract information from the 64-bit word
        //data header
        uint32_t hdrValue = word64.header;

        //data value
        //sample 1
        int32_t adc1ValueSigned = word64.adcSamp;

        ////sample 2
        //int32_t adc2ValueSigned = (int32_t) dataFileReader::decodeADCValueSigned(word64, SECOND_ADC_SAMPLE_FIRST_BIT_POS_IN_64_BIT_WORD);
        ////sample 3
        //int32_t adc3ValueSigned = (int32_t) dataFileReader::decodeADCValueSigned(word64, THIRD_ADC_SAMPLE_FIRST_BIT_POS_IN_64_BIT_WORD);

        //bit representation
        vector <int32_t> dataSigned;
        dataSigned.push_back(adc1ValueSigned);
        //                dataSigned.push_back(adc2ValueSigned);
        //                dataSigned.push_back(adc3ValueSigned);
        for (uint adcDataInd = 0; adcDataInd<dataSigned.size(); adcDataInd++) {

            if (gVerbosityTranslate)
            {
                LOG_F(INFO, "%d %d %llu %d %llu", counterValue, idValue, hdrValue, dataSigned[adcDataInd], word64.word64);
            }
            //file csv file
            myfile << counterValue << ", " << idValue << ", " << hdrValue << ", "<< dataSigned[adcDataInd] << "\n";

            //fill root file
#ifdef USEROOT
            dataEntry.cntr = counterValue;
            dataEntry.hdr = hdrValue;
            dataEntry.id = idValue;
            dataEntry.data = dataSigned[adcDataInd];
            dataTree->Fill();
#endif
        }//end for 3 samples in one 64-bit word

    } //while

    //aca cerrar loop de varios archivos
    myfile.close();
#ifdef USEROOT
    dataTree->Write();
    fout->Close();
#endif

    if (reader.getDataCount()==SMART_BUFFER_SIZE) {
        LOG_F(INFO, "data good: correct number of samples");
        if (deleteDats_) reader.deleteDatFiles();
    }

    LOG_F(INFO, "number of adc data = %lld", reader.getDataCount());
    return 0;
}


int DataConversion::bin_to_calibrate(const string outFileName, const unsigned int regMinCol, const unsigned int regMaxCol, const unsigned int regMinRow, const unsigned int regMaxRow, const unsigned int imageNCols, const vector<imageVar> vars)
{
    LOG_F(INFO, "start calibration of interleaving gains");

    //image variables
    //ch1
    vector<vector <float>> pixelsVector;
    vector <int> channelIndex;
    int nChs = 0;

    dataFileReader reader(inFileNames_, gVerbosityTranslate);

    ////64-bit word from packer
    dataword_t word64;
    while (reader.getWord(word64)) {
        uint8_t idValue = word64.chID;
        int32_t dataValueSigned = word64.cdsSamp;

        //SAVE PIXEL INFORMATION HERE
        if (idValue>=channelIndex.size()) {
            while (channelIndex.size()<idValue) channelIndex.push_back(-1);
            channelIndex.push_back(nChs);
            pixelsVector.push_back(vector <float> ());
            nChs ++;
        }
        pixelsVector[channelIndex[idValue]].push_back(dataValueSigned);

    } //while


    //calculate the necesary images
    float newPedValue = 0;
    float newSigValue = 0;
    for (uint imageInd = 0; imageInd < pixelsVector.size(); ++imageInd)
    {
        vector<vector <float>> pixelFinValue(LTA_NUMBER_OF_CHANNELS/2 + 1, vector <float> ());
        for (uint pixelInd = 0; pixelInd<pixelsVector[0].size(); pixelInd = pixelInd + INTERLEAVING_NUM_INTEG_PER_PIXEL) {

            //generate de desired information and calculate gains
            if (imageInd == 0 || imageInd ==1) {
                //pixel information of channels
                newPedValue = pixelsVector[imageInd][pixelInd+0];
                newSigValue = pixelsVector[imageInd][pixelInd+1];
                pixelFinValue[0].push_back(-1.*(newSigValue - newPedValue));
                //noise information of channel 2
                newPedValue = pixelsVector[2][pixelInd+0];
                newSigValue = pixelsVector[2][pixelInd+1];
                pixelFinValue[1].push_back(-1.*(newSigValue - newPedValue));
                //noise information of channel 3
                newPedValue = pixelsVector[3][pixelInd+0];
                newSigValue = pixelsVector[3][pixelInd+1];
                pixelFinValue[2].push_back(-1.*(newSigValue - newPedValue));
            }else if(imageInd == 2 || imageInd ==3) {
                //pixel information of channels
                newPedValue = pixelsVector[imageInd][pixelInd+1];
                newSigValue = pixelsVector[imageInd][pixelInd+2];
                pixelFinValue[0].push_back(-1.*(newSigValue - newPedValue));
                //noise information of channel 2
                newPedValue = pixelsVector[0][pixelInd+1];
                newSigValue = pixelsVector[0][pixelInd+2];
                pixelFinValue[1].push_back(-1.*(newSigValue - newPedValue));
                //noise information of channel 3
                newPedValue = pixelsVector[1][pixelInd+1];
                newSigValue = pixelsVector[1][pixelInd+2];
                pixelFinValue[2].push_back(-1.*(newSigValue - newPedValue));
            }
        }
        //save images used for calibration
        for (uint imageFitsInd = 0; imageFitsInd < pixelFinValue.size(); ++imageFitsInd)
        {
            stringstream outFString;
            outFString << outFileName << "_calibration_ch_" << imageInd << "_" << imageFitsInd << ".fits";

            //create fits image
            int status = 0;

            lastFilename_ = outFString.str();
            long nRows = pixelFinValue[imageFitsInd].size()/imageNCols;
            FitsWriter outf(1, imageNCols, nRows, outFString.str(), false, false);
            outf.create_img(vars);
            status = outf.dump_to_fits(1, pixelFinValue[imageFitsInd]);

            //output image is closed when FitsWriter goes out of scope
            if (status != 0) {
                return status;
            }
        }

        //get region of interest
        vector<vector <double>> pixGoodReg(LTA_NUMBER_OF_CHANNELS/2 + 1, vector <double> ());
        unsigned long vectorInd = 0;
        for (uint row = regMinRow; row<=regMaxRow; row++) {
            for (uint col =  regMinCol; col<=regMaxCol; col++) {
                vectorInd = col + (row-1)*imageNCols;
                pixGoodReg[0].push_back(pixelFinValue[0][vectorInd]);
                pixGoodReg[1].push_back(pixelFinValue[1][vectorInd]);
                pixGoodReg[2].push_back(pixelFinValue[2][vectorInd]);
            }
        }

        //calculate gain
        double cov01 = 0;
        covariance(pixGoodReg[0], pixGoodReg[1], cov01);
        double cov02 = 0;
        covariance(pixGoodReg[0], pixGoodReg[2], cov02);
        double cov11 = 0;
        covariance(pixGoodReg[1], pixGoodReg[1], cov11);
        double cov22 = 0;
        covariance(pixGoodReg[2], pixGoodReg[2], cov22);
        double cov12 = 0;
        covariance(pixGoodReg[1], pixGoodReg[2], cov12);
        double cov21 = 0;
        covariance(pixGoodReg[2], pixGoodReg[1], cov21);
        double determinant = cov11*cov22 - cov12*cov21;
        LOG_F(INFO, "determinant %f %f %f %f %f", determinant, cov02, cov12, cov01, cov22);
        gainMatrix[imageInd][0] = (cov02*cov12 - cov01*cov22)/determinant;
        gainMatrix[imageInd][1] = (cov12*cov01 - cov11*cov02)/determinant;

    }

    print_interleaving();

    return 0;
}

void DataConversion::print_interleaving() {
    //print calculated gains
    LOG_F(INFO, "CALCULATED INTERLEAVING GAINS====================================");
    for (uint i = 0; i<gainMatrix.size(); i++) {
        LOG_F(INFO, "channel %d: %f and %f", i, gainMatrix[i][0], gainMatrix[i][1]);
    }
    LOG_F(INFO, "=================================================================");
}

