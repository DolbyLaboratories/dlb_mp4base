#/************************************************************************************************************
# * Copyright (c) 2017, Dolby Laboratories Inc.
# * All rights reserved.
#
# * Redistribution and use in source and binary forms, with or without modification, are permitted
# * provided that the following conditions are met:
#
# * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
# *    and the following disclaimer.
# * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
# *    and the following disclaimer in the documentation and/or other materials provided with the distribution.
# * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or
# *    promote products derived from this software without specific prior written permission.
#
# * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
# * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
# * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# * OF THE POSSIBILITY OF SUCH DAMAGE.
# ************************************************************************************************************/

#-*-makefile-*-

# colorized output can be disabled (e.g. for log files) by calling make with COLOR=0
ifneq ($(COLOR),0)
COL_OUTPUT=\033[33m
COL_END=\033[0m
endif

# to dump the complete compiler/linker/archiver commandlines, call make with VERBOSE=1
ifeq ($(VERBOSE),1)
QUIET=
else
AT=@
QUIET=--quiet
endif

ECHO=echo
PRINTF=printf
MKDIR_P=mkdir -p

ifeq ($(OS),Windows_NT)
ifneq ($(TERM),cygwin)
RM=del
endif
endif

all: mp4muxer_release mp4muxer_debug

.PHONY: help force
force: ;
help:
	$(AT)$(ECHO) "This makefile has the following targets:"
	$(AT)$(ECHO) "	all"
	$(AT)$(ECHO) "	mp4muxer_release"
	$(AT)$(ECHO) "	mp4muxer_debug"
	$(AT)$(ECHO) "	clean"


BASE=../../../../

# Compile files for mp4muxer_release
CC_mp4muxer_release=$(CC)
CFLAGS_OUTPUT_FILE_mp4muxer_release=-o 
CFLAGS_mp4muxer_release=\
  $(EXTRA_CFLAGS) \
  -O3 \
  -Wvla \
  -Wdeclaration-after-statement \
  -std=gnu99 \
  -pedantic \
  -Wall \
  -c \
  -DENABLE_MP4_MSGLOG=1 \
  -DNDEBUG=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/frontend

CCDEP_mp4muxer_release=$(CC)
CCDEPFLAGS_mp4muxer_release=\
  -MM \
  -DENABLE_MP4_MSGLOG=1 \
  -DNDEBUG=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/frontend \
  -MT

CCDEPFLAGS_OUTPUT_FILE_mp4muxer_release=-o 
OBJS_mp4muxer_release=\
  obj/mp4muxer_release/mp4_muxer_app.o \
  obj/mp4muxer_release/ema_mp4_mux_api.o

DEPS_mp4muxer_release=\
  obj/mp4muxer_release/mp4_muxer_app.d \
  obj/mp4muxer_release/ema_mp4_mux_api.d


obj/mp4muxer_release:
	$(AT)$(MKDIR_P) obj/mp4muxer_release



include $(wildcard obj/mp4muxer_release/mp4_muxer_app.d)

    
obj/mp4muxer_release/mp4_muxer_app.o: $(BASE)dlb_mp4base/frontend/mp4_muxer_app.c | obj/mp4muxer_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4muxer_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4muxer_release) $(CCDEPFLAGS_mp4muxer_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4muxer_release)obj/mp4muxer_release/mp4_muxer_app.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4muxer_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4muxer_release) $(CFLAGS_mp4muxer_release) $(CFLAGS_OUTPUT_FILE_mp4muxer_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4muxer_release/ema_mp4_mux_api.d)

    
obj/mp4muxer_release/ema_mp4_mux_api.o: $(BASE)dlb_mp4base/frontend/ema_mp4_mux_api.c | obj/mp4muxer_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4muxer_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4muxer_release) $(CCDEPFLAGS_mp4muxer_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4muxer_release)obj/mp4muxer_release/ema_mp4_mux_api.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4muxer_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4muxer_release) $(CFLAGS_mp4muxer_release) $(CFLAGS_OUTPUT_FILE_mp4muxer_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"





# Compile files for mp4muxer_debug
CC_mp4muxer_debug=$(CC)
CFLAGS_OUTPUT_FILE_mp4muxer_debug=-o 
CFLAGS_mp4muxer_debug=\
  $(EXTRA_CFLAGS) \
  -g \
  -ggdb3 \
  -O0 \
  -Wvla \
  -Wdeclaration-after-statement \
  -std=gnu99 \
  -pedantic \
  -Wall \
  -c \
  -DDEBUG=1 \
  -DENABLE_MP4_MSGLOG=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/frontend

CCDEP_mp4muxer_debug=$(CC)
CCDEPFLAGS_mp4muxer_debug=\
  -MM \
  -DDEBUG=1 \
  -DENABLE_MP4_MSGLOG=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/frontend \
  -MT

CCDEPFLAGS_OUTPUT_FILE_mp4muxer_debug=-o 
OBJS_mp4muxer_debug=\
  obj/mp4muxer_debug/mp4_muxer_app.o \
  obj/mp4muxer_debug/ema_mp4_mux_api.o

DEPS_mp4muxer_debug=\
  obj/mp4muxer_debug/mp4_muxer_app.d \
  obj/mp4muxer_debug/ema_mp4_mux_api.d


obj/mp4muxer_debug:
	$(AT)$(MKDIR_P) obj/mp4muxer_debug



include $(wildcard obj/mp4muxer_debug/mp4_muxer_app.d)

    
obj/mp4muxer_debug/mp4_muxer_app.o: $(BASE)dlb_mp4base/frontend/mp4_muxer_app.c | obj/mp4muxer_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4muxer_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4muxer_debug) $(CCDEPFLAGS_mp4muxer_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4muxer_debug)obj/mp4muxer_debug/mp4_muxer_app.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4muxer_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4muxer_debug) $(CFLAGS_mp4muxer_debug) $(CFLAGS_OUTPUT_FILE_mp4muxer_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/mp4muxer_debug/ema_mp4_mux_api.d)

    
obj/mp4muxer_debug/ema_mp4_mux_api.o: $(BASE)dlb_mp4base/frontend/ema_mp4_mux_api.c | obj/mp4muxer_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_mp4muxer_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_mp4muxer_debug) $(CCDEPFLAGS_mp4muxer_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_mp4muxer_debug)obj/mp4muxer_debug/ema_mp4_mux_api.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_mp4muxer_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_mp4muxer_debug) $(CFLAGS_mp4muxer_debug) $(CFLAGS_OUTPUT_FILE_mp4muxer_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"





LD_mp4muxer_release=gcc
LDFLAGS_mp4muxer_release=$(EXTRA_LDFLAGS)  -O2
LDLIBS_mp4muxer_release=-lm
LDFLAGS_OUTPUT_FILE_mp4muxer_release=-o 

# Link mp4muxer_release
mp4muxer_release: $(OBJS_mp4muxer_release) $(BASE)dlb_mp4base/make/libmp4base/macos/libmp4base_release.a
	$(AT)$(ECHO) "[LD:gcc] $^"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(LD_mp4muxer_release) $(LDFLAGS_mp4muxer_release) $(LDFLAGS_OUTPUT_FILE_mp4muxer_release)$@ $^ $(LDLIBS_mp4muxer_release)
	$(AT)$(PRINTF) "$(COL_END)"

$(BASE)dlb_mp4base/make/libmp4base/macos/libmp4base_release.a: force
	$(AT)make $(QUIET) --no-print-directory -C $(BASE)dlb_mp4base/make/libmp4base/macos libmp4base_release.a



LD_mp4muxer_debug=gcc
LDFLAGS_mp4muxer_debug=$(EXTRA_LDFLAGS)  -rdynamic
LDLIBS_mp4muxer_debug=-lm
LDFLAGS_OUTPUT_FILE_mp4muxer_debug=-o 

# Link mp4muxer_debug
mp4muxer_debug: $(OBJS_mp4muxer_debug) $(BASE)dlb_mp4base/make/libmp4base/macos/libmp4base_debug.a
	$(AT)$(ECHO) "[LD:gcc] $^"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(LD_mp4muxer_debug) $(LDFLAGS_mp4muxer_debug) $(LDFLAGS_OUTPUT_FILE_mp4muxer_debug)$@ $^ $(LDLIBS_mp4muxer_debug)
	$(AT)$(PRINTF) "$(COL_END)"

$(BASE)dlb_mp4base/make/libmp4base/macos/libmp4base_debug.a: force
	$(AT)make $(QUIET) --no-print-directory -C $(BASE)dlb_mp4base/make/libmp4base/macos libmp4base_debug.a



clean:
	$(RM) $(OBJS_mp4muxer_release)
	$(RM) $(DEPS_mp4muxer_release)
	$(RM) $(OBJS_mp4muxer_debug)
	$(RM) $(DEPS_mp4muxer_debug)
	$(RM) mp4muxer_release
	make -C $(BASE)dlb_mp4base/make/libmp4base/macos clean
	$(RM) mp4muxer_debug
