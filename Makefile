# Copyright 2020, JP Norair
#
# Licensed under the OpenTag License, Version 1.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

CC := gcc
LD := ld

THISMACHINE ?= $(shell uname -srm | sed -e 's/ /-/g')
THISSYSTEM	?= $(shell uname -s)
TARGET      ?= $(THISMACHINE)

EXT_INC     ?= 
EXT_LIBINC  ?= 
EXT_LIBFLAGS?=
EXT_LIBS    ?= 

VERSION     ?= 0.5.0
PACKAGEDIR  ?= ./../_hbpkg/$(THISMACHINE)/clithread.$(VERSION)

ifeq ($(THISSYSTEM),Darwin)
# Mac can't do conditional selection of static and dynamic libs at link time.
#	PRODUCTS := libclithread.dylib libclithread.a
	PRODUCTS := libclithread.a
	EXT_INC := -I/opt/homebrew/include $(EXT_INC)
	EXT_LIBINC := -L/opt/homebrew/lib $(EXT_LIBINC)
else ifeq ($(THISSYSTEM),Linux)
	PRODUCTS := libclithread.so libclithread.a
else ifeq ($(THISSYSTEM),CYGWIN_NT-10.0)
	PRODUCTS := libclithread.a
else
	error "THISSYSTEM set to unknown value: $(THISSYSTEM)"
endif

ifeq ($(MAKECMDGOALS),debug)
	APPDIR      := bin/$(THISMACHINE)
	BUILDDIR    := build/$(THISMACHINE)_debug
	DEBUG_MODE  := 1
else
	APPDIR   := bin/$(THISMACHINE)
	BUILDDIR    := build/$(THISMACHINE)
	DEBUG_MODE  := 0
endif

ifneq ($(DEBUG_MODE),0)
	ifeq ($(DEBUG_MODE),1)
		CFLAGS  ?= -std=gnu99 -O2 -Wall -pthread -D__DEBUG__
	else
		CFLAGS  ?= -std=gnu99 -O -g -Wall -pthread -D__DEBUG__
	endif
	SRCEXT      := c
	DEPEXT      := dd
	OBJEXT      := do
else 
	CFLAGS      ?= -std=gnu99 -O3 -pthread
	SRCEXT      := c
	DEPEXT      := d
	OBJEXT      := o
endif

SRCDIR      := .
INCDIR      := .
RESDIR      := 
LIB         := $(EXT_LIBINC) -ltalloc $(EXT_LIBFLAGS)
INC         := -I$(INCDIR) -I./../_hbsys/$(TARGET)/include $(EXT_INC) 
INCDEP      := -I$(INCDIR) -I./../_hbsys/$(TARGET)/include $(EXT_INC) 
LIBMODULES  := hbutils $(EXT_LIBS)

#SOURCES     := $(shell find $(SRCDIR) -type f -name "*.$(SRCEXT)")
SOURCES     := $(shell ls $(SRCDIR)/*.$(SRCEXT))
OBJECTS     := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))



all: lib
deps: $(LIBMODULES)
lib: resources $(PRODUCTS)
remake: cleaner all
pkg: deps lib install
debug: resources libclithread.a
pkgdebug: debug install


install:
	@rm -rf $(PACKAGEDIR)
	@mkdir -p $(PACKAGEDIR)
	@rm -f $(PACKAGEDIR)/../clithread
	@cp -R $(APPDIR)/* $(PACKAGEDIR)/
	@cp -R ./*.h $(PACKAGEDIR)/
	@ln -s clithread.$(VERSION) ./$(PACKAGEDIR)/../clithread
	cd ../_hbsys && $(MAKE) sys_install INS_MACHINE=$(THISMACHINE) INS_PKGNAME=clithread

#Copy Resources from Resources Directory to Target Directory
resources: directories

#Make the Directories
directories:
	@mkdir -p $(APPDIR)
	@mkdir -p $(BUILDDIR)

#Clean only Objects
clean:
	@$(RM) -rf $(BUILDDIR)

#Full Clean, Objects and Binaries
cleaner: clean
	@$(RM) -rf libclithread.a
	@$(RM) -rf $(APPDIR)

#Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))
	
#Build the dynamic library
libclithread.so: $(OBJECTS)
	$(CC) -shared -fPIC -Wl,-soname,libclithread.so.1 -o $(APPDIR)/$@.$(VERSION) $(OBJECTS) -lc

libclithread.dylib: $(OBJECTS)
	$(CC) -dynamiclib -o $(APPDIR)/$@ $(OBJECTS)

#Build static library -- same on all POSIX.  Also has debug version
libclithread.a: $(OBJECTS)
	ar -rcs $(APPDIR)/$@ $(OBJECTS)
	ranlib $(APPDIR)/$@

libclithread.debug.a: $(OBJECTS)
	ar -rcs $(APPDIR)/$@ $(OBJECTS)
	ranlib $(APPDIR)/$@


#Library dependencies (not in local sources)
$(LIBMODULES): %: 
	cd ./../$@ && $(MAKE) pkg


#Compile
$(BUILDDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<
	@$(CC) $(CFLAGS) $(INCDEP) -MM $(SRCDIR)/$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT)
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp

#Non-File Targets
.PHONY: all lib pkg debug pkgdebug remake clean cleaner resources


