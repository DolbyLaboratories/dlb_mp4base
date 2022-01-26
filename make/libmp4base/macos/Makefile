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

all: libmp4base_release.a libmp4base_debug.a

.PHONY: help force
force: ;
help:
	$(AT)$(ECHO) "This makefile has the following targets:"
	$(AT)$(ECHO) "	all"
	$(AT)$(ECHO) "	libmp4base_release.a"
	$(AT)$(ECHO) "	libmp4base_debug.a"
	$(AT)$(ECHO) "	clean"


BASE=../../../../

# Compile files for libmp4base_release
CC_libmp4base_release=$(CC)
CFLAGS_OUTPUT_FILE_libmp4base_release=-o 
CFLAGS_libmp4base_release=\
  $(EXTRA_CFLAGS) \
  -O3 \
  -Wvla \
  -Wdeclaration-after-statement \
  -std=gnu99 \
  -pedantic \
  -Wall \
  -c \
  -DEMA_BUILD=1 \
  -DNDEBUG=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include

CCDEP_libmp4base_release=$(CC)
CCDEPFLAGS_libmp4base_release=\
  -MM \
  -DEMA_BUILD=1 \
  -DNDEBUG=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -MT

CCDEPFLAGS_OUTPUT_FILE_libmp4base_release=-o 
OBJS_libmp4base_release=\
  obj/libmp4base_release/dsi.o \
  obj/libmp4base_release/parser_aac.o \
  obj/libmp4base_release/parser_avc.o \
  obj/libmp4base_release/parser_avc_dec.o \
  obj/libmp4base_release/parser_avc_dpb.o \
  obj/libmp4base_release/parser_hevc.o \
  obj/libmp4base_release/parser_hevc_dec.o \
  obj/libmp4base_release/parser.o \
  obj/libmp4base_release/parser_dd.o \
  obj/libmp4base_release/parser_ac4.o \
  obj/libmp4base_release/mp4_isom.o \
  obj/libmp4base_release/mp4_muxer.o \
  obj/libmp4base_release/mp4_stream.o \
  obj/libmp4base_release/io_base.o \
  obj/libmp4base_release/io_buffer.o \
  obj/libmp4base_release/io_file.o \
  obj/libmp4base_release/list_itr.o \
  obj/libmp4base_release/msg_log.o \
  obj/libmp4base_release/registry.o \
  obj/libmp4base_release/utils.o

DEPS_libmp4base_release=\
  obj/libmp4base_release/dsi.d \
  obj/libmp4base_release/parser_aac.d \
  obj/libmp4base_release/parser_avc.d \
  obj/libmp4base_release/parser_avc_dec.d \
  obj/libmp4base_release/parser_avc_dpb.d \
  obj/libmp4base_release/parser_hevc.d \
  obj/libmp4base_release/parser_hevc_dec.d \
  obj/libmp4base_release/parser.d \
  obj/libmp4base_release/parser_dd.d \
  obj/libmp4base_release/parser_ac4.d \
  obj/libmp4base_release/mp4_isom.d \
  obj/libmp4base_release/mp4_muxer.d \
  obj/libmp4base_release/mp4_stream.d \
  obj/libmp4base_release/io_base.d \
  obj/libmp4base_release/io_buffer.d \
  obj/libmp4base_release/io_file.d \
  obj/libmp4base_release/list_itr.d \
  obj/libmp4base_release/msg_log.d \
  obj/libmp4base_release/registry.d \
  obj/libmp4base_release/utils.d


obj/libmp4base_release:
	$(AT)$(MKDIR_P) obj/libmp4base_release



include $(wildcard obj/libmp4base_release/dsi.d)

    
obj/libmp4base_release/dsi.o: $(BASE)dlb_mp4base/src/esparser/dsi.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/dsi.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/parser_aac.d)

    
obj/libmp4base_release/parser_aac.o: $(BASE)dlb_mp4base/src/esparser/parser_aac.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/parser_aac.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/parser_avc.d)

    
obj/libmp4base_release/parser_avc.o: $(BASE)dlb_mp4base/src/esparser/parser_avc.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/parser_avc.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/parser_avc_dec.d)

    
obj/libmp4base_release/parser_avc_dec.o: $(BASE)dlb_mp4base/src/esparser/parser_avc_dec.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/parser_avc_dec.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/parser_avc_dpb.d)

    
obj/libmp4base_release/parser_avc_dpb.o: $(BASE)dlb_mp4base/src/esparser/parser_avc_dpb.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/parser_avc_dpb.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/parser_hevc.d)

    
obj/libmp4base_release/parser_hevc.o: $(BASE)dlb_mp4base/src/esparser/parser_hevc.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/parser_hevc.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/parser_hevc_dec.d)

    
obj/libmp4base_release/parser_hevc_dec.o: $(BASE)dlb_mp4base/src/esparser/parser_hevc_dec.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/parser_hevc_dec.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/parser.d)

    
obj/libmp4base_release/parser.o: $(BASE)dlb_mp4base/src/esparser/parser.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/parser.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/parser_dd.d)

    
obj/libmp4base_release/parser_dd.o: $(BASE)dlb_mp4base/src/esparser/parser_dd.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/parser_dd.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"



include $(wildcard obj/libmp4base_release/parser_ac4.d)

    
obj/libmp4base_release/parser_ac4.o: $(BASE)dlb_mp4base/src/esparser/parser_ac4.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/parser_ac4.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/mp4_isom.d)

    
obj/libmp4base_release/mp4_isom.o: $(BASE)dlb_mp4base/src/mp4_isom.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/mp4_isom.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/mp4_muxer.d)

    
obj/libmp4base_release/mp4_muxer.o: $(BASE)dlb_mp4base/src/mp4_muxer.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/mp4_muxer.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/mp4_stream.d)

    
obj/libmp4base_release/mp4_stream.o: $(BASE)dlb_mp4base/src/mp4_stream.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/mp4_stream.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/io_base.d)

    
obj/libmp4base_release/io_base.o: $(BASE)dlb_mp4base/src/util/io_base.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/io_base.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/io_buffer.d)

    
obj/libmp4base_release/io_buffer.o: $(BASE)dlb_mp4base/src/util/io_buffer.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/io_buffer.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/io_file.d)

    
obj/libmp4base_release/io_file.o: $(BASE)dlb_mp4base/src/util/io_file.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/io_file.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/list_itr.d)

    
obj/libmp4base_release/list_itr.o: $(BASE)dlb_mp4base/src/util/list_itr.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/list_itr.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/msg_log.d)

    
obj/libmp4base_release/msg_log.o: $(BASE)dlb_mp4base/src/util/msg_log.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/msg_log.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/registry.d)

    
obj/libmp4base_release/registry.o: $(BASE)dlb_mp4base/src/util/registry.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/registry.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_release/utils.d)

    
obj/libmp4base_release/utils.o: $(BASE)dlb_mp4base/src/util/utils.c | obj/libmp4base_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_release) $(CCDEPFLAGS_libmp4base_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_release)obj/libmp4base_release/utils.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_release) $(CFLAGS_libmp4base_release) $(CFLAGS_OUTPUT_FILE_libmp4base_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"





# Compile files for libmp4base_debug
CC_libmp4base_debug=$(CC)
CFLAGS_OUTPUT_FILE_libmp4base_debug=-o 
CFLAGS_libmp4base_debug=\
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
  -DEMA_BUILD=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include

CCDEP_libmp4base_debug=$(CC)
CCDEPFLAGS_libmp4base_debug=\
  -MM \
  -DDEBUG=1 \
  -DEMA_BUILD=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -MT

CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug=-o 
OBJS_libmp4base_debug=\
  obj/libmp4base_debug/dsi.o \
  obj/libmp4base_debug/parser_aac.o \
  obj/libmp4base_debug/parser_avc.o \
  obj/libmp4base_debug/parser_avc_dec.o \
  obj/libmp4base_debug/parser_avc_dpb.o \
  obj/libmp4base_debug/parser_hevc.o \
  obj/libmp4base_debug/parser_hevc_dec.o \
  obj/libmp4base_debug/parser.o \
  obj/libmp4base_debug/parser_dd.o \
  obj/libmp4base_debug/parser_ac4.o \
  obj/libmp4base_debug/mp4_isom.o \
  obj/libmp4base_debug/mp4_muxer.o \
  obj/libmp4base_debug/mp4_stream.o \
  obj/libmp4base_debug/io_base.o \
  obj/libmp4base_debug/io_buffer.o \
  obj/libmp4base_debug/io_file.o \
  obj/libmp4base_debug/list_itr.o \
  obj/libmp4base_debug/msg_log.o \
  obj/libmp4base_debug/registry.o \
  obj/libmp4base_debug/utils.o

DEPS_libmp4base_debug=\
  obj/libmp4base_debug/dsi.d \
  obj/libmp4base_debug/parser_aac.d \
  obj/libmp4base_debug/parser_avc.d \
  obj/libmp4base_debug/parser_avc_dec.d \
  obj/libmp4base_debug/parser_avc_dpb.d \
  obj/libmp4base_debug/parser_hevc.d \
  obj/libmp4base_debug/parser_hevc_dec.d \
  obj/libmp4base_debug/parser.d \
  obj/libmp4base_debug/parser_dd.d \
  obj/libmp4base_debug/parser_ac4.d \
  obj/libmp4base_debug/mp4_isom.d \
  obj/libmp4base_debug/mp4_muxer.d \
  obj/libmp4base_debug/mp4_stream.d \
  obj/libmp4base_debug/io_base.d \
  obj/libmp4base_debug/io_buffer.d \
  obj/libmp4base_debug/io_file.d \
  obj/libmp4base_debug/list_itr.d \
  obj/libmp4base_debug/msg_log.d \
  obj/libmp4base_debug/registry.d \
  obj/libmp4base_debug/utils.d


obj/libmp4base_debug:
	$(AT)$(MKDIR_P) obj/libmp4base_debug



include $(wildcard obj/libmp4base_debug/dsi.d)

    
obj/libmp4base_debug/dsi.o: $(BASE)dlb_mp4base/src/esparser/dsi.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/dsi.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/parser_aac.d)

    
obj/libmp4base_debug/parser_aac.o: $(BASE)dlb_mp4base/src/esparser/parser_aac.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/parser_aac.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/parser_avc.d)

    
obj/libmp4base_debug/parser_avc.o: $(BASE)dlb_mp4base/src/esparser/parser_avc.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/parser_avc.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/parser_avc_dec.d)

    
obj/libmp4base_debug/parser_avc_dec.o: $(BASE)dlb_mp4base/src/esparser/parser_avc_dec.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/parser_avc_dec.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/parser_avc_dpb.d)

    
obj/libmp4base_debug/parser_avc_dpb.o: $(BASE)dlb_mp4base/src/esparser/parser_avc_dpb.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/parser_avc_dpb.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/parser_hevc.d)

    
obj/libmp4base_debug/parser_hevc.o: $(BASE)dlb_mp4base/src/esparser/parser_hevc.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/parser_hevc.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/parser_hevc_dec.d)

    
obj/libmp4base_debug/parser_hevc_dec.o: $(BASE)dlb_mp4base/src/esparser/parser_hevc_dec.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/parser_hevc_dec.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/parser.d)

    
obj/libmp4base_debug/parser.o: $(BASE)dlb_mp4base/src/esparser/parser.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/parser.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/parser_dd.d)

    
obj/libmp4base_debug/parser_dd.o: $(BASE)dlb_mp4base/src/esparser/parser_dd.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/parser_dd.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/parser_ac4.d)

    
obj/libmp4base_debug/parser_ac4.o: $(BASE)dlb_mp4base/src/esparser/parser_ac4.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/parser_ac4.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"



include $(wildcard obj/libmp4base_debug/mp4_isom.d)

    
obj/libmp4base_debug/mp4_isom.o: $(BASE)dlb_mp4base/src/mp4_isom.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/mp4_isom.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/mp4_muxer.d)

    
obj/libmp4base_debug/mp4_muxer.o: $(BASE)dlb_mp4base/src/mp4_muxer.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/mp4_muxer.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/mp4_stream.d)

    
obj/libmp4base_debug/mp4_stream.o: $(BASE)dlb_mp4base/src/mp4_stream.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/mp4_stream.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"



include $(wildcard obj/libmp4base_debug/io_base.d)

    
obj/libmp4base_debug/io_base.o: $(BASE)dlb_mp4base/src/util/io_base.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/io_base.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/io_buffer.d)

    
obj/libmp4base_debug/io_buffer.o: $(BASE)dlb_mp4base/src/util/io_buffer.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/io_buffer.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/io_file.d)

    
obj/libmp4base_debug/io_file.o: $(BASE)dlb_mp4base/src/util/io_file.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/io_file.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/list_itr.d)

    
obj/libmp4base_debug/list_itr.o: $(BASE)dlb_mp4base/src/util/list_itr.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/list_itr.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/msg_log.d)

    
obj/libmp4base_debug/msg_log.o: $(BASE)dlb_mp4base/src/util/msg_log.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/msg_log.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/registry.d)

    
obj/libmp4base_debug/registry.o: $(BASE)dlb_mp4base/src/util/registry.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/registry.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/libmp4base_debug/utils.d)

    
obj/libmp4base_debug/utils.o: $(BASE)dlb_mp4base/src/util/utils.c | obj/libmp4base_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_libmp4base_debug) $(CCDEPFLAGS_libmp4base_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_libmp4base_debug)obj/libmp4base_debug/utils.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_libmp4base_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_libmp4base_debug) $(CFLAGS_libmp4base_debug) $(CFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"





AR_libmp4base_release=ar
ARFLAGS_libmp4base_release=-rc
ARFLAGS_OUTPUT_FILE_libmp4base_release= 

libmp4base_release.a: $(OBJS_libmp4base_release)
	$(AT)$(ECHO) "[AR:$(AR_libmp4base_release)] $@"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(AR_libmp4base_release) $(ARFLAGS_libmp4base_release) $(ARFLAGS_OUTPUT_FILE_libmp4base_release)$@ $^
	$(AT)$(PRINTF) "$(COL_END)"

AR_libmp4base_debug=ar
ARFLAGS_libmp4base_debug=-rc
ARFLAGS_OUTPUT_FILE_libmp4base_debug= 

libmp4base_debug.a: $(OBJS_libmp4base_debug)
	$(AT)$(ECHO) "[AR:$(AR_libmp4base_debug)] $@"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(AR_libmp4base_debug) $(ARFLAGS_libmp4base_debug) $(ARFLAGS_OUTPUT_FILE_libmp4base_debug)$@ $^
	$(AT)$(PRINTF) "$(COL_END)"

clean:
	$(RM) $(OBJS_libmp4base_release)
	$(RM) $(DEPS_libmp4base_release)
	$(RM) $(OBJS_libmp4base_debug)
	$(RM) $(DEPS_libmp4base_debug)
	$(RM) libmp4base_release.a
	$(RM) libmp4base_debug.a
