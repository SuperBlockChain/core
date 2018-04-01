## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = serial-tests
bin_PROGRAMS = snb-core

include $(top_srcdir)/common.mk
include $(srcdir)/src.mk

noinst_HEADERS = $(SRC_H_FILES)

snb_core_SOURCES = $(SRC_CXX_FILES)
snb_core_LDADD = $(soci_LIBS) $(libmedida_LIBS)		\
	$(top_builddir)/lib/lib3rdparty.a $(sqlite3_LIBS)	\
	$(libpq_LIBS) $(xdrpp_LIBS) $(libsodium_LIBS)

TESTDATA_DIR = testdata
TEST_FILES = $(TESTDATA_DIR)/snb-core_example.cfg $(TESTDATA_DIR)/snb-core_standalone.cfg $(TESTDATA_DIR)/snb-core_testnet.cfg

BUILT_SOURCES = $(SRC_X_FILES:.x=.h) SuperBlockChainCoreVersion.h $(TEST_FILES)

SUFFIXES = .x .h
.x.h: $(XDRC)
	$(XDRC) -hh -o $@ $<

$(srcdir)/src.mk: $(top_srcdir)/make-mks
	cd $(top_srcdir) && ./make-mks

$(TESTDATA_DIR)/%.cfg : $(top_srcdir)/docs/%.cfg
	mkdir -p $(@D) && cp $< $@

.PHONY: always
always:
	@:

# Always rebuild because .git/HEAD is a symbolic ref one can't depend on
SuperBlockChainCoreVersion.h: always
	@vers=$$(cd "$(srcdir)" \
		&& git describe --always --dirty --tags 2>/dev/null \
		|| echo "$(PACKAGE) $(VERSION)"); \
	    echo "#define STELLAR_CORE_VERSION \"$$vers\"" > $@~
	@if cmp -s $@~ $@; then rm -f $@~; else \
	    mv -f $@~ $@ && printf "echo '%s' > $@\n" "$$(cat $@)"; fi

if USE_POSTGRES
TESTS=test/selftest-pg
else # !USE_POSTGRES
TESTS=test/selftest-nopg
endif # !USE_POSTGRES

if USE_CLANG_FORMAT
format: always
	cd $(srcdir) && $(CLANG_FORMAT) -i $(SRC_CXX_FILES) $(SRC_H_FILES)
endif # USE_CLANG_FORMAT

if USE_AFL_FUZZ
fuzz-testcases: snb-core
	mkdir -p fuzz-testcases
	for i in `seq 1 10`; do \
	    ./snb-core --genfuzz fuzz-testcases/fuzz$$i.xdr; \
	done

fuzz: fuzz-testcases snb-core
	mkdir -p fuzz-findings
	afl-fuzz -m 8000 -t 250 -i fuzz-testcases -o fuzz-findings \
	    ./snb-core --fuzz @@

fuzz-clean: always
	rm -Rf fuzz-testcases fuzz-findings

distclean-local: fuzz-clean
endif # USE_AFL_FUZZ

CLEANFILES = $(BUILT_SOURCES) *~ */*~ snb*.log
MAINTAINERCLEANFILES = $(srcdir)/Makefile.in $(srcdir)/*~ $(srcdir)/*/*~

EXTRA_DIST = $(SRC_X_FILES) test/selftest-nopg test/selftest-pg
