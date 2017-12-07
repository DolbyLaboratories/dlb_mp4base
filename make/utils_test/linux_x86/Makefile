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

all: utils_test_release utils_test_debug

.PHONY: help force
force: ;
help:
	$(AT)$(ECHO) "This makefile has the following targets:"
	$(AT)$(ECHO) "	all"
	$(AT)$(ECHO) "	utils_test_release"
	$(AT)$(ECHO) "	utils_test_debug"
	$(AT)$(ECHO) "	clean"


BASE=../../../../

# Compile files for utils_test_release
CC_utils_test_release=$(CC)
CFLAGS_OUTPUT_FILE_utils_test_release=-o 
CFLAGS_utils_test_release=\
  $(EXTRA_CFLAGS) \
  -O3 \
  -Wvla \
  -Wdeclaration-after-statement \
  -std=gnu99 \
  -pedantic \
  -Wall \
  -c \
  -DNDEBUG=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/test/unit \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include

CCDEP_utils_test_release=$(CC)
CCDEPFLAGS_utils_test_release=\
  -MM \
  -DNDEBUG=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/test/unit \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -MT

CCDEPFLAGS_OUTPUT_FILE_utils_test_release=-o 
OBJS_utils_test_release=\
  obj/utils_test_release/utils_test.o \
  obj/utils_test_release/test_util.o

DEPS_utils_test_release=\
  obj/utils_test_release/utils_test.d \
  obj/utils_test_release/test_util.d


obj/utils_test_release:
	$(AT)$(MKDIR_P) obj/utils_test_release



include $(wildcard obj/utils_test_release/utils_test.d)

    
obj/utils_test_release/utils_test.o: $(BASE)dlb_mp4base/test/unit/utils_test.c | obj/utils_test_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_utils_test_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_utils_test_release) $(CCDEPFLAGS_utils_test_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_utils_test_release)obj/utils_test_release/utils_test.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_utils_test_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_utils_test_release) $(CFLAGS_utils_test_release) $(CFLAGS_OUTPUT_FILE_utils_test_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/utils_test_release/test_util.d)

    
obj/utils_test_release/test_util.o: $(BASE)dlb_mp4base/test/unit/test_util.c | obj/utils_test_release
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_utils_test_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_utils_test_release) $(CCDEPFLAGS_utils_test_release) $@ $(CCDEPFLAGS_OUTPUT_FILE_utils_test_release)obj/utils_test_release/test_util.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_utils_test_release)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_utils_test_release) $(CFLAGS_utils_test_release) $(CFLAGS_OUTPUT_FILE_utils_test_release)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"





# Compile files for utils_test_debug
CC_utils_test_debug=$(CC)
CFLAGS_OUTPUT_FILE_utils_test_debug=-o 
CFLAGS_utils_test_debug=\
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
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/test/unit \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include

CCDEP_utils_test_debug=$(CC)
CCDEPFLAGS_utils_test_debug=\
  -MM \
  -DDEBUG=1 \
  -I$(BASE). \
  -I$(BASE)dlb_mp4base/test/unit \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -I$(BASE)dlb_mp4base/include \
  -MT

CCDEPFLAGS_OUTPUT_FILE_utils_test_debug=-o 
OBJS_utils_test_debug=\
  obj/utils_test_debug/utils_test.o \
  obj/utils_test_debug/test_util.o

DEPS_utils_test_debug=\
  obj/utils_test_debug/utils_test.d \
  obj/utils_test_debug/test_util.d


obj/utils_test_debug:
	$(AT)$(MKDIR_P) obj/utils_test_debug



include $(wildcard obj/utils_test_debug/utils_test.d)

    
obj/utils_test_debug/utils_test.o: $(BASE)dlb_mp4base/test/unit/utils_test.c | obj/utils_test_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_utils_test_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_utils_test_debug) $(CCDEPFLAGS_utils_test_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_utils_test_debug)obj/utils_test_debug/utils_test.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_utils_test_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_utils_test_debug) $(CFLAGS_utils_test_debug) $(CFLAGS_OUTPUT_FILE_utils_test_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"


include $(wildcard obj/utils_test_debug/test_util.d)

    
obj/utils_test_debug/test_util.o: $(BASE)dlb_mp4base/test/unit/test_util.c | obj/utils_test_debug
	$(AT)$(ECHO) "[CCDEP:$(CCDEP_utils_test_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CCDEP_utils_test_debug) $(CCDEPFLAGS_utils_test_debug) $@ $(CCDEPFLAGS_OUTPUT_FILE_utils_test_debug)obj/utils_test_debug/test_util.d $<
	$(AT)$(PRINTF) "$(COL_END)"
	$(AT)$(ECHO) "[CC:$(CC_utils_test_debug)] $<"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(CC_utils_test_debug) $(CFLAGS_utils_test_debug) $(CFLAGS_OUTPUT_FILE_utils_test_debug)$@ $<
	$(AT)$(PRINTF) "$(COL_END)"





LD_utils_test_release=gcc
LDFLAGS_utils_test_release=$(EXTRA_LDFLAGS) -O2
LDLIBS_utils_test_release=-lm
LDFLAGS_OUTPUT_FILE_utils_test_release=-o 

# Link utils_test_release
utils_test_release: $(OBJS_utils_test_release) $(BASE)dlb_mp4base/make/libmp4base/linux_x86/libmp4base_release.a
	$(AT)$(ECHO) "[LD:gcc] $^"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(LD_utils_test_release) $(LDFLAGS_utils_test_release) $(LDFLAGS_OUTPUT_FILE_utils_test_release)$@ $^ $(LDLIBS_utils_test_release)
	$(AT)$(PRINTF) "$(COL_END)"

$(BASE)dlb_mp4base/make/libmp4base/linux_x86/libmp4base_release.a: force
	$(AT)make $(QUIET) --no-print-directory -C $(BASE)dlb_mp4base/make/libmp4base/linux_x86 libmp4base_release.a



LD_utils_test_debug=gcc
LDFLAGS_utils_test_debug=$(EXTRA_LDFLAGS) -rdynamic
LDLIBS_utils_test_debug=-lm
LDFLAGS_OUTPUT_FILE_utils_test_debug=-o 

# Link utils_test_debug
utils_test_debug: $(OBJS_utils_test_debug) $(BASE)dlb_mp4base/make/libmp4base/linux_x86/libmp4base_debug.a
	$(AT)$(ECHO) "[LD:gcc] $^"
	$(AT)$(PRINTF) "$(COL_OUTPUT)"
	$(AT)$(LD_utils_test_debug) $(LDFLAGS_utils_test_debug) $(LDFLAGS_OUTPUT_FILE_utils_test_debug)$@ $^ $(LDLIBS_utils_test_debug)
	$(AT)$(PRINTF) "$(COL_END)"

$(BASE)dlb_mp4base/make/libmp4base/linux_x86/libmp4base_debug.a: force
	$(AT)make $(QUIET) --no-print-directory -C $(BASE)dlb_mp4base/make/libmp4base/linux_x86 libmp4base_debug.a



clean:
	$(RM) $(OBJS_utils_test_release)
	$(RM) $(DEPS_utils_test_release)
	$(RM) $(OBJS_utils_test_debug)
	$(RM) $(DEPS_utils_test_debug)
	$(RM) utils_test_release
	make -C $(BASE)dlb_mp4base/make/libmp4base/linux_x86 clean
	$(RM) utils_test_debug
