CROSS_PREFIX ?= x86_64-w64-mingw32-
CXX := $(CROSS_PREFIX)g++

SOURCE_DIR := Source
BINARY_DIR := Binaries/Win64
OBJECT_DIR := Intermediate/Build

TARGET := $(BINARY_DIR)/FortExternalServer.exe

SOURCES := $(shell find $(SOURCE_DIR) -name '*.cpp' -not -path '*/Private/Mac/*')
OBJECTS := $(patsubst $(SOURCE_DIR)/%.cpp,$(OBJECT_DIR)/%.o,$(SOURCES))
DEPENDS := $(OBJECTS:.o=.d)

CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter -municode -MMD -MP \
	-I$(SOURCE_DIR) \
	-DWIN32_LEAN_AND_MEAN -DNOMINMAX -DUNICODE -D_UNICODE

LDFLAGS := -municode -static-libgcc -static-libstdc++
LDLIBS := -lpsapi -lole32 -lshlwapi

.PHONY: all clean rebuild

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

$(OBJECT_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECT_DIR) $(BINARY_DIR)

rebuild: clean all

-include $(DEPENDS)
