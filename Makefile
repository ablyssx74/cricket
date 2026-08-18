# Target application name
TARGET = cricket
PACKAGE_DIR := build/package
NAME = cricket
VERSION = 0.0.59
REVISION = 1

# --- 2. Architecture & Paths ---
UNAME_M := $(shell uname -p)
ifeq ($(UNAME_M), BePC)
    CXX = g++-x86
    ARCH = x86_gcc2
    LIB_ARCH_DIR = /x86
    LDFLAGS = -L/boot/system/lib/x86 -Wl,--gc-sections -s
    is32bit = _x86
    DEFINES += -DIS_HAIKU_32BIT
else
    CXX = g++
    ARCH = x86_64
    LIB_ARCH_DIR = 
    LDFLAGS = -L/boot/system/lib -Wl,--gc-sections -s
endif

DEFINES := $(DEFINES)

# OPTIMIZED CXXFLAGS: Aggressive loop optimizations (-O3), dead-code section generation,
# and warning suppressions to guarantee a clean, flawless compilation terminal output.
CXXFLAGS = -Wall -O3 -fdata-sections -ffunction-sections -Wno-reorder -Wno-unused-but-set-variable -I/boot/system/develop/headers/private/netservices2 

# Source files, objects, and resources
SRCS = cricket.cpp icons.cpp
OBJS = $(SRCS:.cpp=.o)
RDEFS = cricket.rdef
RSRCS = $(RDEFS:.rdef=.rsrc)

# Haiku specific libraries
LIBS = -lbe -lnetwork -lnetservices2 -lbnetapi -lshared -ltranslation -ltracker -lssl -lcrypto -lcurl




# Default target
all: $(TARGET)

# Link the final binary and append resources
$(TARGET): $(OBJS) $(RSRCS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LIBS)
	xres -o $(TARGET) $(RSRCS)
	mimeset -f $(TARGET)

# Compile the resource script into binary format
%.rsrc: %.rdef
	rc -o $@ $<

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(DEFINES) $(CXXFLAGS) -c $< -o $@
	
release: all
	@[ -n "$(PACKAGE_DIR)" ] || { echo "PACKAGE_DIR is undefined"; exit 1; }
	rm -rf "./$(PACKAGE_DIR)"
	mkdir -p $(PACKAGE_DIR)
	sed -e 's/$$(NAME)/$(NAME)/g' -e 's/$$(VERSION)/$(VERSION)/g'  -e 's/$$(REVISION)/$(REVISION)/g' -e 's/$$(is32bit)/$(is32bit)/g' -e 's/$$(ARCH)/$(ARCH)/' -e 's/$$(YEAR)/$(shell date +%Y)/' $(NAME).tpl > $(PACKAGE_DIR)/.PackageInfo
	mkdir -p $(PACKAGE_DIR)/apps
	mkdir -p $(PACKAGE_DIR)/bin
	mkdir -p $(PACKAGE_DIR)/data/deskbar/menu/Applications
	cp $(NAME) $(PACKAGE_DIR)/apps/$(NAME)
	ln -s ../apps/$(NAME) $(PACKAGE_DIR)/bin/$(NAME)
	ln -s ../../../../apps/$(NAME) $(PACKAGE_DIR)/data/deskbar/menu/Applications/$(NAME)
	package create -C $(PACKAGE_DIR) $(NAME)-$(VERSION)-$(REVISION)-$(ARCH).hpkg

# Clean up build files
clean:
	rm -f $(OBJS) $(RSRCS) $(TARGET) *.hpkg
	rm -fr build

.PHONY: all release clean
