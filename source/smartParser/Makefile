ROOT =
CPP = g++
CC = gcc
GLIBS =
GLIBS +=
OBJECTS = \
          smartRecipe.o \
          smartVariable.o \
          smartSequence.o \
          smartState.o \
          smartStep.o \
          tinyxml2.o \
          xmlParser.o \
          main.o

HEADERS = \
          smartRecipe.h \
          smartVariable.h \
          smartSequence.h \
          smartState.h \
          smartStep.h \
          tinyxml2.h \
          xmlParser.h


ALL : parser.exe
	@echo "Listo!"

parser.exe : $(OBJECTS) $(HEADERS)
	$(CPP) $(OBJECTS) -o parser.exe

%.o : %.cpp $(HEADERS)
	$(CPP) -c $< -o $@ $(CFLAGS)

clean:
	rm -f *~ *.o *.exe

