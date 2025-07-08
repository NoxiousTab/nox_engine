# -----------------------------------------------------------------------------
# Makefile for compiling noxengine
# -----------------------------------------------------------------------------

TARGET    := noxengine
_THIS     := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
_ROOT     := $(_THIS)
CXX       := g++
CXXFLAGS  := -std=c++20 -O3 -flto -funroll-loops -fno-exceptions -DNDEBUG -Wall -Wextra -Iinc
NATIVE    := -march=native -mtune=native
SRC_DIR   := src
INC_DIR   := inc
SOURCES   := $(filter-out $(SRC_DIR)/generation.cpp $(SRC_DIR)/traditional.cpp, $(wildcard $(SRC_DIR)/*.cpp))
OBJECTS   := $(patsubst $(SRC_DIR)/%.cpp, $(SRC_DIR)/%.o, $(SOURCES))
DEPENDS   := $(OBJECTS:.o=.d)
EXE       := $(TARGET)

# System dependent flags -------------------------------------------------------

ifeq ($(OS), Windows_NT)
	SYSTEM_OS := Windows
	SUFFIX := .exe
	CXXFLAGS += -static
else
	SYSTEM_OS := $(shell uname -s)
	SUFFIX :=
	CXXFLAGS += -pthread
	LDFLAGS := -pthread
endif

CXX_VERSION := $(shell $(CXX) --version)
ifneq ($(SYSTEM_OS), Darwin)
	ifneq (, $(findstring clang, $(CXX_VERSION)))
		LDFLAGS += -fuse-ld=lld
	endif
endif

# Specific builds -------------------------------------------------------------

ifeq ($(build), debug)
	CXXFLAGS = -std=c++20 -Og -g3 -fno-omit-frame-pointer -Iinc
	ifeq ($(SYSTEM_OS), Windows)
		CXXFLAGS += -fsanitize=undefined,address
	else
		CXXFLAGS += -fsanitize=undefined,address,leak
	endif
endif

ifeq ($(build), datagen)
	CXXFLAGS += -DNOX_GEN
	SOURCES  += $(SRC_DIR)/generation.cpp
	OBJECTS  += $(SRC_DIR)/generation.o
	DEPENDS  += $(SRC_DIR)/generation.d
endif

ifeq ($(build), x86-64)
	NATIVE   = -msse -msse2 -mtune=sandybridge
endif

ifeq ($(build), x86-64-bmi2)
	NATIVE   = -march=haswell -mtune=haswell
endif

# Build targets ---------------------------------------------------------------

all: $(TARGET)

clean:
ifeq ($(SYSTEM_OS), Windows)
	@del /Q $(SRC_DIR)\*.o $(SRC_DIR)\*.d $(EXE).exe 2>nul
else
	@rm -f $(SRC_DIR)/*.o $(SRC_DIR)/*.d $(EXE)
endif

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(NATIVE) -o $(EXE)$(SUFFIX) $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(NATIVE) -MMD -MP -c $< -o $@

-include $(DEPENDS)

$(SRC_DIR)/noxengine.o: FORCE
FORCE:
