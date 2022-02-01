include Makefile.inc
export CYCDIR CYCC EXEC_PREFIX

CYCC=$(CYCDIR)/bin/cyclone$(EXE)
EXEC_PREFIX=$(CYCDIR)/bin/lib/cyc-lib
GC=gc
BANSHEE=banshee
# Target directories:
# BB is where object files for the standard build are put
# BT is where object files for a cross build are put
# BL is where the finished library files are put
# bin is where executables are put
#
BB=build/$(build)
BT=build/$(target)
BL=bin/lib

all: directories cyclone tools xml otherlibs

cyclone-inf: banshee_engine directories cyclone-inf-inner

$(BL)/libcycboot.a: $(BB)/libcycboot.a
	cp -p $< $@

$(BL)/libcycboot_g.a: $(BB)/gdb/libcycboot.a
	cp -p $< $@

$(BL)/libcycboot_a.a: $(BB)/aprof/libcycboot.a
	cp -p $< $@

$(BL)/libcycboot_pg.a: $(BB)/gprof/libcycboot.a
	cp -p $< $@

$(BL)/libcycboot_nocheck.a: $(BB)/nocheck/libcycboot.a
	cp -p $< $@

LIBCYC_PREREQS = bin/cyclone$(EXE) \
  $(BL)/cyc-lib/cyc_include.h $(BL)/cyc-lib/$(build)/cyc_setjmp.h

$(BL)/libcyc.a: $(LIBCYC_PREREQS) $(BB)/libcyc.a include-directory
	cp -p $(BB)/libcyc.a $@

$(BL)/libcyc_g.a: $(LIBCYC_PREREQS) $(BB)/gdb/libcyc.a include-directory
	cp -p $(BB)/gdb/libcyc.a $@

$(BL)/libcyc_a.a: $(LIBCYC_PREREQS) $(BB)/aprof/libcyc.a include-directory
	cp -p $(BB)/aprof/libcyc.a $@

$(BL)/libcyc_pg.a: $(LIBCYC_PREREQS) $(BB)/gprof/libcyc.a include-directory
	cp -p $(BB)/gprof/libcyc.a $@

$(BL)/libcyc_nocheck.a: $(LIBCYC_PREREQS) $(BB)/nocheck/libcyc.a include-directory
	cp -p $(BB)/nocheck/libcyc.a $@

$(BL)/libxml.a: $(BB)/libxml.a
	cp -p $< $@

$(BL)/libxml_g.a: $(BB)/gdb/libxml.a
	cp -p $< $@

$(BL)/libxml_a.a: $(BB)/aprof/libxml.a
	cp -p $< $@

$(BL)/libxml_pg.a: $(BB)/gprof/libxml.a
	cp -p $< $@

$(BL)/libxml_nocheck.a: $(BB)/nocheck/libxml.a
	cp -p $< $@

$(BL)/cyc-lib/%/nogc.a: build/%/nogc.a
	cp -p $< $@

$(BL)/cyc-lib/%/nogc_g.a: build/%/gdb/nogc.a
	cp -p $< $@

$(BL)/cyc-lib/%/nogc_a.a: build/%/aprof/nogc.a
	cp -p $< $@

$(BL)/cyc-lib/%/nogc_pg.a: build/%/gprof/nogc.a
	cp -p $< $@

$(BL)/cyc-lib/%/nogc_nocheck.a: build/%/nocheck/nogc.a
	cp -p $< $@

ifeq ($(HAVE_PTHREAD),yes)
$(BL)/cyc-lib/%/nogc_pthread.a: build/%/pthread/nogc.a
	cp -p $< $@

gc_pthread:
	cp -r $(GC) $@
	(cd $@; $(MAKE) clean; ./configure --enable-threads=pthreads --enable-shared=no --enable-cplusplus=no)

.PRECIOUS: gc_pthread gc_pthread/.libs/libgc.a

gc_pthread/.libs/libgc.a: gc_pthread
	$(MAKE) -C $^ libgc.la CC="$(CC)" CFLAGS="$(CFLAGS)"

$(BL)/cyc-lib/%/gc_pthread.a: gc_pthread/.libs/libgc.a
	cp -p $< $@
endif

$(BL)/cyc-lib/%/runtime_cyc.a: build/%/runtime_cyc.a
	cp -p $< $@

$(BL)/cyc-lib/%/runtime_cyc_g.a: build/%/gdb/runtime_cyc.a
	cp -p $< $@

$(BL)/cyc-lib/%/runtime_cyc_a.a: build/%/aprof/runtime_cyc.a
	cp -p $< $@

$(BL)/cyc-lib/%/runtime_cyc_pg.a: build/%/gprof/runtime_cyc.a
	cp -p $< $@

$(BL)/cyc-lib/%/runtime_cyc_nocheck.a: build/%/nocheck/runtime_cyc.a
	cp -p $< $@

$(BL)/cyc-lib/%/runtime_cyc_pthread.a: build/%/pthread/runtime_cyc.a
	cp -p $< $@

bin/cyclone-inf$(EXE): $(BB)/cyclone-inf$(EXE) \
  $(BL)/cyc-lib/cyc_include.h $(BL)/cyc-lib/$(build)/cyc_setjmp.h
	cp $< $@

bin/cyclone$(EXE): $(BB)/cyclone$(EXE) \
  $(BL)/cyc-lib/cyc_include.h $(BL)/cyc-lib/$(build)/cyc_setjmp.h
	cp $< $@

bin/cyclone_g$(EXE): $(BB)/gdb/cyclone$(EXE) \
  $(BL)/cyc-lib/cyc_include.h $(BL)/cyc-lib/$(build)/cyc_setjmp.h
	cp $< $@

bin/cyclone_a$(EXE): $(BB)/aprof/cyclone$(EXE) \
  $(BL)/cyc-lib/cyc_include.h $(BL)/cyc-lib/$(build)/cyc_setjmp.h
	cp $< $@

bin/cyclone_pg$(EXE): $(BB)/gprof/cyclone$(EXE) \
  $(BL)/cyc-lib/cyc_include.h $(BL)/cyc-lib/$(build)/cyc_setjmp.h
	cp $< $@

bin/cyclone_nocheck$(EXE): $(BB)/nocheck/cyclone$(EXE) \
  $(BL)/cyc-lib/cyc_include.h $(BL)/cyc-lib/$(build)/cyc_setjmp.h
	cp $< $@

bin/cycdoc$(EXE): $(BB)/cycdoc$(EXE)
	cp $< $@

bin/buildlib$(EXE):: $(BB)/buildlib$(EXE) \
  $(BL)/cyc-lib/cyc_include.h $(BL)/cyc-lib/$(build)/cycspecs
	cp $(BB)/buildlib$(EXE) $@

# NB: the cp -p is used to preserve the time on the include files,
# which are prereqs of some .o files, namely, in tools/yakker.
# Without -p, those .o files would be out of date on every make.
include-directory:
	for i in `(cd $(BB)/include; find * -type d)`;\
	  do mkdir -p $(BL)/cyc-lib/$(build)/include/$$i; done
	for i in `(cd $(BB)/include; find * -name '*.h')`;\
	  do cp -p $(BB)/include/$$i $(BL)/cyc-lib/$(build)/include/$$i; done

# Directory structure of the installed library.  (During boot,
# this is built in bin/lib ($(BL)).)
#
# lib/
#   libcyc.a                  # "Regular" lib files go here as in gcc
#   libcycboot.a              # .. maybe the boot lib isn't regular?
#   cyc-lib/                  # "Special" compiler files go here
#     cyc_include.h           # Non-arch specific
#     ARCH/                   # i686-unknown-linux, etc.
#       cyc_setjmp.h          # These are all arch specific
#       cycspecs
#       gc.a
#       include/              # Arch specific headers; non-arch spec elsewhere
#         HEADERS             # stdio.h, etc.
#       nogc.a
#       runtime_cyc.a

directories::
	@mkdir -p $(BL)/cyc-lib/$(build)/include
	@mkdir -p $(BB)
	@mkdir -p $(BB)/include
	@mkdir -p $(BB)/gdb
	@mkdir -p $(BB)/aprof
	@mkdir -p $(BB)/gprof
	@mkdir -p $(BB)/nocheck
	@mkdir -p $(BB)/pthread
ifneq ($(build),$(target))
	@mkdir -p $(BT)
	@mkdir -p $(BT)/include
endif

# FIX: we are not using the cycspecs file in the same way that
# gcc specs files are used, probably there should be just one
# entry for cyclone instead of separate entries for
# cyclone_target_cflags, cyclone_cc, etc.
$(BL)/cyc-lib/$(build)/cycspecs: config/buildspecs
	echo "*cyclone_target_cflags:" > $@
	echo "  $(CFLAGS)" >> $@
	echo "" >> $@
	echo "*cyclone_cc:" >> $@
	echo "  $(CC)" >> $@
	echo "" >> $@
	echo "*cyclone_inc_path:" >> $@
	echo "  $(INC_INSTALL)" >> $@
	echo "" >> $@
	config/buildspecs "$(CC)" >> $@

$(BL)/cyc-lib/$(build)/cyc_setjmp.h: bin/cyc-lib/libc.cys bin/buildlib$(EXE)
	bin/buildlib$(EXE) -B$(BL)/cyc-lib -d $(BB)/include -setjmp > $@

$(BL)/cyc-lib/cyc_include.h: bin/cyc-lib/cyc_include.h
	cp $< $@

$(BL)/cyc-lib/$(build)/gc.a: $(GC)/.libs/libgc.a
	cp -p $< $@

$(GC)/.libs/libgc.a:
	$(MAKE) -C $(GC) libgc.la CC="$(CC)" CFLAGS="$(CFLAGS)"

# Print version to standard output -- used by makedist
version:
	@echo $(VERSION)

tools: cyclone
	$(MAKE) -C tools/stringify install
	$(MAKE) -C tools/bison install
	$(MAKE) -C tools/cyclex install
	$(MAKE) -C tools/flex install
	$(MAKE) -C tools/rewrite install
	$(MAKE) -C tools/errorgen install
	$(MAKE) -C tools/yakker cyclone-install

otherlibs: cyclone
ifeq ($(HAVE_PCRE),yes)
	$(MAKE) -C otherlibs/pcre install
endif
ifeq ($(HAVE_SQLITE3),yes)
	$(MAKE) -C otherlibs/sqlite3 install
endif

cyclone: \
  directories \
  bin/buildlib$(EXE) \
  bin/cyclone$(EXE) \
  $(BL)/libcycboot.a \
  $(BL)/libcyc.a \
  $(BL)/cyc-lib/$(build)/nogc.a \
  $(BL)/cyc-lib/$(build)/runtime_cyc.a \
  bin/cycdoc$(EXE)

banshee_engine:
	$(MAKE) CC=$(CC) -C $(BANSHEE) banshee

cyclone-inf-inner: \
  directories \
  bin/cyclone-inf$(EXE)

ifeq ($(HAVE_PTHREAD),yes)
cyclone: \
  $(BL)/cyc-lib/$(build)/runtime_cyc_pthread.a \
  $(BL)/cyc-lib/$(build)/nogc_pthread.a \
  $(BL)/cyc-lib/$(build)/gc_pthread.a
endif

cyclone-aprof: aprof bin/cyclone_a$(EXE)

aprof: \
  $(BL)/libcycboot_a.a \
  $(BL)/libcyc_a.a \
  $(BL)/cyc-lib/$(build)/nogc_a.a \
  $(BL)/cyc-lib/$(build)/runtime_cyc_a.a
	$(MAKE) -C tools/aprof install

cyclone-gprof: gprof bin/cyclone_pg$(EXE)

gprof: \
  $(BL)/libcycboot_pg.a \
  $(BL)/libcyc_pg.a \
  $(BL)/cyc-lib/$(build)/nogc_pg.a \
  $(BL)/cyc-lib/$(build)/runtime_cyc_pg.a

cyclone-nocheck: nocheck bin/cyclone_nocheck$(EXE)

nocheck: \
  $(BL)/libcycboot_nocheck.a \
  $(BL)/libcyc_nocheck.a \
  $(BL)/cyc-lib/$(build)/nogc_nocheck.a \
  $(BL)/cyc-lib/$(build)/runtime_cyc_nocheck.a

cyclone-gdb: gdb bin/cyclone_g$(EXE)

# Currently, the -g flag to cyclone does not use these libraries.
# We include the make rules in case we want to use them in the future.
gdb: \
  $(BL)/libcycboot_g.a \
  $(BL)/libcyc_g.a \
  $(BL)/cyc-lib/$(build)/nogc_g.a \
  $(BL)/cyc-lib/$(build)/runtime_cyc_g.a

ifndef NO_XML_LIB
xml:     tools $(BL)/libxml.a
aprof:   tools $(BL)/libxml_a.a
gprof:   tools $(BL)/libxml_pg.a
nocheck: tools $(BL)/libxml_nocheck.a
gdb: tools $(BL)/libxml_g.a
endif

.PHONY: all tools cyclone aprof gprof nocheck directories xml
.PHONY: include-directory include-target-directory
.PHONY: cyclone-aprof cyclone-gprof cyclone-nocheck

########################################################################
# FOR BUILDING A CROSS COMPILER                                        #
########################################################################

# If a cross gcc is not installed you can still do some sanity
# checks on the build process by doing
#
#   make target=XXXXXX TARGET_CC=gcc TARGET_CFLAGS=-DXXXXXX BTARGET=-DZZZZZZ all
#
ifneq ($(build),$(target))

$(BL)/cyc-lib/$(target)/libcycboot.a: $(BT)/libcycboot.a
	cp -p $< $@

LIBCYC_TARGET_PREREQS = bin/cyclone$(EXE) \
  $(BL)/cyc-lib/cyc_include.h $(BL)/cyc-lib/$(target)/cyc_setjmp.h

$(BL)/cyc-lib/$(target)/libcyc.a: $(LIBCYC_TARGET_PREREQS) \
  $(BT)/libcyc.a include-target-directory
	cp -p $(BT)/libcyc.a $@

$(BL)/cyc-lib/$(target)/libxml.a: $(BT)/libxml.a
	cp -p $< $@

BTARGET=-b $(target)

bin/buildlib$(EXE):: $(BL)/cyc-lib/$(target)/cycspecs

# NB: as a side-effect this creates the include directory
include-target-directory:
	for i in `(cd $(BT)/include; find * -type d)`;\
	  do mkdir -p $(BL)/cyc-lib/$(target)/include/$$i; done
	for i in `(cd $(BT)/include; find * -name '*.h')`;\
	  do cp -p $(BT)/include/$$i $(BL)/cyc-lib/$(target)/include/$$i; done

directories::
	@mkdir -p $(BL)/cyc-lib/$(target)/include

$(BL)/cyc-lib/$(target)/cycspecs: config/buildspecs
	echo "*cyclone_target_cflags:" > $@
	echo "  $(TARGET_CFLAGS)" >> $@
	echo "" >> $@
	echo "*cyclone_cc:" >> $@
	echo "  $(TARGET_CC)" >> $@
	echo "" >> $@
	echo "*cyclone_inc_path:" >> $@
	echo "  $(INC_INSTALL)" >> $@
	echo "" >> $@
	config/buildspecs $(TARGET_CC) >> $@

$(BL)/cyc-lib/$(target)/cyc_setjmp.h: bin/cyc-lib/libc.cys bin/buildlib$(EXE)
	bin/buildlib$(EXE) $(BTARGET) -B$(BL)/cyc-lib -d $(BT)/include -setjmp > $@

# FIX: can we cross compile the gc?
cyclone: \
  $(BL)/cyc-lib/$(target)/libcycboot.a \
  $(BL)/cyc-lib/$(target)/libcyc.a \
  $(BL)/cyc-lib/$(target)/nogc.a \
  $(BL)/cyc-lib/$(target)/runtime_cyc.a

ifeq ($(HAVE_PTHREAD),yes)
cyclone: \
  $(BL)/cyc-lib/$(target)/nogc_pthread.a \
  $(BL)/cyc-lib/$(target)/runtime_cyc_pthread.a
endif

ifndef NO_XML_LIB
xml:     $(BL)/cyc-lib/$(target)/libxml.a
endif

endif
#########################################################################
# END CROSS COMPILE                                                     #
#########################################################################



# Store the compiler, libraries, and tools in the user-defined directories.
# Also, keep a record of what was copied for later uninstall.
install: all inc_install lib_install bin_install
uninstall: inc_uninstall lib_uninstall bin_uninstall

ifdef INC_INSTALL
inc_install:
	$(SHELL) config/cyc_install include/* $(INC_INSTALL)
inc_uninstall:
	$(SHELL) config/cyc_install -u $(INC_INSTALL)
else
inc_install inc_uninstall:
	@(echo "no include directory specified"; exit 1)
endif
ifdef BIN_INSTALL
bin_install:
	$(SHELL) config/cyc_install bin/cyclone$(EXE) bin/cyclone-inf$(EXE) bin/cycbison$(EXE) bin/cyclex$(EXE) bin/cycflex$(EXE) bin/rewrite$(EXE) bin/aprof$(EXE) bin/errorgen$(EXE) bin/stringify$(EXE) bin/yakker$(EXE) $(BIN_INSTALL)
bin_uninstall:
	$(SHELL) config/cyc_install -u $(BIN_INSTALL)
else
bin_install bin_uninstall:
	@(echo "no bin directory specified"; exit 1)
endif
ifdef LIB_INSTALL
lib_install:
	$(SHELL) config/cyc_install $(BL)/* $(LIB_INSTALL)
lib_uninstall:
	$(SHELL) config/cyc_install -u $(LIB_INSTALL)
else
lib_install lib_uninstall:
	@(echo "no lib directory specified"; exit 1)
endif
.PHONY: install uninstall inc_install lib_install bin_install
.PHONY: inc_uninstall lib_uninstall bin_uninstall

BUILDDIR=build/boot
GENDIR=bin/genfiles

# These build off the Cyclone source files, but do not replace anything in bin
# We override BUILDDIR and CYCFLAGS for many nefarious purposes:
#   cross-compiling, bootstrapping, profiling, ...
# We use the -r flag to eliminate built-in make rules.  This is needed
#   because of a circular dependence that arises when bootstrapping as with
#   make cyclone_src BUILDDIR=build/boot1 CYCC=`pwd`/build/boot/cyclone
ifdef DEBUGBUILD
LIBSRC_FLAGS += DEBUGBUILD=X
endif
ifdef PROFILE
LIBSRC_FLAGS += PROFILE=X
endif
ifdef ALLOC_PROFILE
LIBSRC_FLAGS += ALLOC_PROFILE=X
endif
ifdef NOGC
LIBSRC_FLAGS += NOGC=X
endif
ifdef NOREGIONS
LIBSRC_FLAGS += NOREGIONS=X
endif
ifdef OPTBUILD
LIBSRC_FLAGS += OPTBUILD=X
endif
DO_LIBSRC=$(MAKE) -r -C $(BUILDDIR) -f $(CYCDIR)/Makefile_libsrc

DO_SRCINF=$(MAKE) -r -C $(BUILDDIR) -f $(CYCDIR)/Makefile_srcinf

src_inf_stats:
	$(DO_SRCINF) inf_stats

clean_src_inf:
	$(DO_SRCINF) clean_src_inf

cyclone_src_inf: cyclone-inf cyclone_src
	$(DO_SRCINF) all

cyclone_src:
	-mkdir -p $(BUILDDIR)
	$(DO_LIBSRC) all

lib_src:
	-mkdir -p $(BUILDDIR)
	$(DO_LIBSRC) libs
cfiles:
	-mkdir -p $(BUILDDIR)
	$(DO_LIBSRC) cfiles

# Note: Tried doing this stuff with target-specific variables instead
#       of recursive invocations, but it crashes gnumake 3.79.1
#       due to "export CYCFLAGS" and "dbg_src: override CYCFLAGS+=-g".
#       They say it's been fixed in their working version.  The crash
#       happens only when the rule (e.g. dbg_src) fires.
dbg_src:
	$(MAKE) BUILDDIR=build/dbg CYCFLAGS="$(CYCFLAGS) -g" cyclone_src
dbg_lib_src:
	$(MAKE) BUILDDIR=build/dbg CYCFLAGS="$(CYCFLAGS) -g" lib_src

# The update files that go from BUILDDIR to GENDIR.
BUILDDIR_UPDATE_FILES=$(UPDATE_SRCS) $(C_BOOT_LIBS) precore_c.h boot_cycstubs.c

# The update files that go from "lib" to GENDIR.
LIB_UPDATE_FILES=boot_cstubs.c nogc.c malloc.c cycinf.c $(C_RUNTIME) runtime_internal.h bget.h

# The update files that go from "lib" to "bin/cyc-lib".
CYC_LIB_UPDATE_FILES=cyc_include.h libc.cys

# This target compares the C files in bin/genfiles to those in src
# Lack of difference means running the update would have no real effect.
XS=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
cmp:
	@for i in $(BUILDDIR_UPDATE_FILES);\
	  do (cmp -s $(GENDIR)/$$i $(BUILDDIR)/$$i\
	      || echo $(XS) $$i CHANGED) done
	@for i in $(CYCLONE_H);\
	  do (test ! -e lib/$$i || cmp -s include/$$i lib/$$i\
              || echo $(XS) lib/$$i CHANGED) done
	@for i in $(LIB_UPDATE_FILES);\
          do (cmp -s $(GENDIR)/$$i lib/$$i\
              || echo $(XS) $$i CHANGED) done
	@for i in $(CYC_LIB_UPDATE_FILES);\
          do (test ! -e lib/$$i\
	      || cmp -s bin/cyc-lib/$$i lib/$$i\
              || echo $(XS) $$i CHANGED) done

# This target updates what is in bin/genfiles and include.
# It would be "dangerous" to invoke this target if we did not have
# version control.  Only updates changed files (makes cvs faster).
update: cfiles
	@for i in $(BUILDDIR_UPDATE_FILES);\
           do (cmp -s $(BUILDDIR)/$$i $(GENDIR)/$$i\
               || (echo UPDATING $(GENDIR)/$$i;\
	           cp $(BUILDDIR)/$$i $(GENDIR)/$$i)) done
	@for i in $(LIB_UPDATE_FILES);\
           do (cmp -s lib/$$i $(GENDIR)/$$i\
               || (echo UPDATING $(GENDIR)/$$i;\
                   cp lib/$$i $(GENDIR)/$$i)) done
	@for i in $(CYCLONE_H);\
           do (test ! -e lib/$$i || cmp -s lib/$$i include/$$i\
               || (echo UPDATING include/$$i;\
                   mv lib/$$i include/$$i)) done
	@for i in $(CYC_LIB_UPDATE_FILES);\
           do (test ! -e lib/$$i\
               || cmp -s lib/$$i bin/cyc-lib/$$i\
               || (echo UPDATING bin/cyc-lib/$$i;\
                   mv lib/$$i bin/cyc-lib/$$i)) done

test_src_compiler: cyclone_src
	@$(RM) -r build/boot_test_src_compiler
	$(MAKE) cyclone_src BUILDDIR=build/boot_test_src_compiler CYCC=`pwd`/$(BUILDDIR)/cyclone
	@cmp -s build/boot_test_src_compiler/cyclone `pwd`/$(BUILDDIR)/cyclone || \
            (echo "XXXXXXXXX Compiler cannot build itself correctly." && false)

# a little testing (much more is in the tests directory)
test:
	$(MAKE) -C tests CYCFLAGS="-g -save-c -pp"
test_bin:
	$(MAKE) -C tests\
         CYCC=$(CYCDIR)/bin/cyclone$(EXE)\
         CYCBISON=$(CYCDIR)/bin/cycbison$(EXE)\
         CYCFLAGS="-L$(CYCDIR)/$(BL) -g -save-c -pp -I$(CYCDIR)/include -B$(CYCDIR)/$(BL)/cyc-lib"
# The -I and -B flags are explained in Makefile_libsrc and should be
# kept in sync with the settings there
test_boot:
	$(MAKE) -C tests\
         CYCC=$(CYCDIR)/build/boot/cyclone$(EXE)\
         CYCBISON=$(CYCDIR)/bin/cycbison$(EXE)\
         CYCFLAGS="$(addprefix -I$(CYCDIR)/build/boot/, . include) $(addprefix -I$(CYCDIR)/, lib src include) -B$(CYCDIR)/build/boot -B$(CYCDIR)/lib -g -save-c -pp"\
	 LDFLAGS="-L$(CYCDIR)/build/boot -B$(CYCDIR)/$(BL)/cyc-lib -v"

clean_test:
	$(MAKE) -C tests clean

# To do: a much safer and less kludgy way to clean build!
# To do: a way to clean individual directories in build.
clean_build:
	mv build/CVS .build_CVS; \
	EXITC=$$?; \
	$(RM) -rf build/*; \
	if [ "$$EXITC" = 0 ]; then \
	mv .build_CVS build/CVS; \
	fi

clean_nogc: clean_test clean_build
	$(MAKE) -C tools/bison  clean
	$(MAKE) -C tools/cyclex clean
	$(MAKE) -C tools/flex   clean
	$(MAKE) -C tools/rewrite clean
	$(MAKE) -C tools/aprof  clean
	$(MAKE) -C tools/errorgen clean
	$(MAKE) -C tools/stringify clean
	$(MAKE) -C tools/yakker clean
	$(MAKE) -C tests        clean
#	$(MAKE) -C doc          clean
	$(MAKE) -C lib/xml      clean
	$(MAKE) -C otherlibs/pcre clean
	$(RM) -r $(BL)
	$(RM) $(addprefix bin/, $(addsuffix $(EXE), cyclone cyclone_g cyclone_a cyclone_pg cyclone_nocheck cycdoc buildlib cycbison cyclex cycflex aprof rewrite errorgen stringify))
	$(RM) *~ amon.out

clean: clean_nogc
	$(MAKE) clean -C $(BANSHEE)
	$(MAKE) clean -C $(GC)
	$(RM) $(GC)/*.exe $(GC)/base_lib $(GC)/*.obj $(GC)/gc.lib
ifeq ($(HAVE_PTHREAD),yes)
	$(RM) -r gc_pthread
endif

include Makefile_base
