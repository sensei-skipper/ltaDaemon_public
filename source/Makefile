CPP = g++
CC = gcc
OS := $(shell uname -s)

CFLAGS = $(shell pkg-config cfitsio --cflags) -g -O3
LIBS = $(shell pkg-config cfitsio --libs)

# build with ROOT only if present
# we check for root-config which should exist on both local and system-wide installations
# https://stackoverflow.com/questions/5618615/check-if-a-program-exists-from-a-makefile
ROOTCONFIG := $(shell command -v root-config 2> /dev/null)
ifneq (${ROOTCONFIG},)
    $(info Found root-config at ${ROOTCONFIG}, building with ROOT)
    CFLAGS += $(shell root-config --cflags) -DUSEROOT=1
    LIBS += $(shell root-config --libs)
else
    $(info Did not find root-config, building without ROOT)
    # these are dependencies which are also dependencies of ROOT
    LIBS += -lpthread -ldl
endif

CFLAGS += -Wall -Wpedantic
CFLAGS += -DLOGURU_UNSAFE_SIGNAL_HANDLER=0 #disable stack trace on signals, otherwise we sometimes have trouble killing the daemon


#add -std flag only if it doesn't already exist (on newer ROOT versions, root-config adds -std)
ifeq (,$(findstring -std=,$(CFLAGS)))
    CFLAGS += -std=c++11
endif
ifeq ($(OS), Linux)
    LIBS += -lrt
endif

OBJECTS = userInterpreter.o ethernet.o dumpToDisc.o dataConversion.o sequencerHandler.o smartSequencerHandler.o udpSocket.o tcpSocket.o ltaBoard.o commandLineArgs.o helperFunctions.o masterLtaClass.o masterLtaCmdLine.o tinyxml2.o loguru.o
HEADERS = userInterpreter.h ethernet.h dumpToDisc.h dataConversion.h sequencerHandler.h smartSequencerHandler.h commandLineArgs.h baseSocket.h udpSocket.h tcpSocket.h ltaBoard.h helperFunctions.h ltaSimulator.h masterLtaClass.h masterLtaCmdLine.h tinyxml2.h loguru.hpp


ALL : ../configure.exe ../ltaSimulator.exe ../masterLta.exe ../readOldDatFiles.exe ../seqTools.exe
	@echo "Listo!"

.SECONDARY: $(OBJECTS)

../%.exe : %.cpp $(OBJECTS) $(HEADERS)
	$(CPP) $< $(OBJECTS) -o $@ $(LIBS) $(CFLAGS)

%.o : %.cpp $(HEADERS)
	$(CPP) -c $< -o $@ $(CFLAGS)

clean:
	rm -f *~ *.o ../*.exe
