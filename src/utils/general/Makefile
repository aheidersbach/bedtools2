OBJ_DIR = ../../../obj/
BIN_DIR = ../../../bin/
UTILITIES_DIR = ../../utils/
# -------------------
# define our includes
# -------------------
INCLUDES = -I$(UTILITIES_DIR)/lineFileUtilities/

# ----------------------------------
# define our source and object files
# ----------------------------------
SOURCES= QuickString.h QuickString.cpp ParseTools.h ParseTools.cpp PushBackStreamBuf.cpp PushBackStreamBuf.h CompressionTools.h CompressionTools.cpp
OBJECTS= QuickString.o ParseTools.o PushBackStreamBuf.o CompressionTools.o
BUILT_OBJECTS= $(patsubst %,$(OBJ_DIR)/%,$(OBJECTS))

all: $(BUILT_OBJECTS)

.PHONY: all

$(BUILT_OBJECTS): $(SOURCES)
	@echo "  * compiling" $(*F).cpp
	@$(CXX) -c -o $@ $(*F).cpp $(LDFLAGS) $(CXXFLAGS) $(INCLUDES)

clean:
	@echo "Cleaning up."
	@rm -f $(OBJ_DIR)/QuickString.o $(OBJ_DIR)/ParseTools.o $(OBJ_DIR)/PushBackStreamBuf.o

.PHONY: clean
