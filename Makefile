# Target application name
TARGET = cricket
PACKAGE_DIR := build/package
NAME = cricket
VERSION = 0.0.43

# Target Arch
UNAME_M := $(shell uname -p)
ifeq ($(UNAME_M), x86)
CXX = g++-x86 
CC = gcc-x86
MAKE := setarch x86 $(MAKE)
ARCH = x86_gcc2
CXXFLAGS := -O2 
INCLUDE = -L/boot/system/lib/x86 
else ifeq ($(UNAME_M), x86_64)
CXX = g++
CC = gcc
ARCH = x86_64
CXXFLAGS := -O3 
INCLUDE = -L/boot/system/lib
endif

# Source files, objects, and resources
SRCS = cricket.cpp icons.cpp
OBJS = $(SRCS:.cpp=.o)
RDEFS = cricket.rdef
RSRCS = $(RDEFS:.rdef=.rsrc)

# Haiku specific libraries
LIBS = -lbe -lnetwork -lnetservices -lbnetapi -lshared -ltranslation -ltracker -lssl -lcrypto 

# Default target
all: $(TARGET)

# Link the final binary and append resources
$(TARGET): $(OBJS) $(RSRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)
	xres -o $(TARGET) $(RSRCS)
	mimeset -f $(TARGET)

# Compile the resource script into binary format
%.rsrc: %.rdef
	rc -o $@ $<

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
	
release: all
	@[ -n "$(PACKAGE_DIR)" ] || { echo "PACKAGE_DIR is undefined"; exit 1; }
	rm -rf "./$(PACKAGE_DIR)"
	mkdir -p $(PACKAGE_DIR)
	sed -e 's/$$(NAME)/$(NAME)/g' -e 's/$$(VERSION)/$(VERSION)/g' -e 's/$$(ARCH)/$(ARCH)/' -e 's/$$(YEAR)/$(shell date +%Y)/' $(NAME).tpl > $(PACKAGE_DIR)/.PackageInfo
	mkdir -p $(PACKAGE_DIR)/apps
	mkdir -p $(PACKAGE_DIR)/bin
	mkdir -p $(PACKAGE_DIR)/data/deskbar/menu/Applications
	cp $(NAME) $(PACKAGE_DIR)/apps/$(NAME)
	ln -s ../apps/$(NAME) $(PACKAGE_DIR)/bin/$(NAME)
	ln -s ../../../../apps/$(NAME) $(PACKAGE_DIR)/data/deskbar/menu/Applications/$(NAME)
	package create -C $(PACKAGE_DIR) $(NAME)-$(VERSION)-1-$(ARCH).hpkg
	
	

# Clean up build files
clean:
	rm -f $(OBJS) $(RSRCS) $(TARGET) *.hpkg
	rm -fr build

.PHONY: all release clean 
