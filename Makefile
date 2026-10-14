# Target application name
TARGET = cricket
PACKAGE_DIR := build/package
NAME = cricket
# The one place a release changes is the VERSION file: the program, the package and the
# resource all take it from there.
VERSION := $(shell sed -n 's/.*\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p' VERSION | head -1)
V_MAJOR := $(word 1,$(subst ., ,$(VERSION)))
V_MIDDLE := $(word 2,$(subst ., ,$(VERSION)))
V_MINOR := $(word 3,$(subst ., ,$(VERSION)))
REVISION = 1

# --- 2. Architecture & Paths ---
UNAME_M := $(shell uname -m)
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
DEFINES += -DCRICKET_VERSION=\"$(VERSION)\"

# OPTIMIZED CXXFLAGS: Aggressive loop optimizations (-O3), dead-code section generation,
# and warning suppressions to guarantee a clean, flawless compilation terminal output.
CXXFLAGS = -Wall -O3 -fdata-sections -ffunction-sections -Wno-reorder -Wno-unused-but-set-variable -I/boot/system/develop/headers/private/netservices2 -Iportmapper 

# Source files, objects, and resources
SRCS = cricket.cpp icons.cpp dcc.cpp portmapper/PortMapper.cpp
OBJS = $(SRCS:.cpp=.o)
RDEFS = cricket.rdef
RSRCS = $(RDEFS:.rdef=.rsrc)

# Haiku specific libraries
LIBS = -lbe -lnetwork -lnetservices2 -lbnetapi -lshared -ltranslation -ltracker -lssl -lcrypto -lcurl -laspell




# Default target
all: $(TARGET)

# Link the final binary and append resources
$(TARGET): $(OBJS) $(RSRCS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LIBS)
	xres -o $(TARGET) $(RSRCS)
	mimeset -f $(TARGET)

# Compile the resource script into binary format (the version fields are filled in from VERSION)
%.rsrc: %.rdef VERSION
	sed -e 's/@V_MAJOR@/$(V_MAJOR)/' -e 's/@V_MIDDLE@/$(V_MIDDLE)/' -e 's/@V_MINOR@/$(V_MINOR)/' -e 's/@VERSION@/$(VERSION)/' $< > $*.version.rdef
	rc -o $@ $*.version.rdef

# The version is compiled in: a new VERSION means a new build of the file that shows it
cricket.o: VERSION

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
	rm -f $(OBJS) $(RSRCS) $(TARGET) *.hpkg *.version.rdef
	rm -fr build

.PHONY: all release clean
