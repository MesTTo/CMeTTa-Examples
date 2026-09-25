# Purpose: discover, compile and run every C example, and hold each language
#   twin against the MeTTa original it mirrors.
# Assumes: CMETTA_DIR holds a built CMeTTa surface and its header, and
#   CMETTA_ENGINE the engine tree whose examples/ are the originals.
# Guarantees: check stops on a failed program, a failed claim, or a twin that
#   disagrees with its original [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
CMETTA_DIR ?= $(abspath ../MeTTa/extensions/cmetta)
CMETTA_ENGINE ?= $(abspath $(CMETTA_DIR)/../..)
.DEFAULT_GOAL := all
CC ?= cc
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -I. -I$(CMETTA_DIR)
LDFLAGS += -L$(CMETTA_DIR) -Wl,-rpath,$(CMETTA_DIR)
LDLIBS += -lcmetta -lm -pthread -ldl
JOBS ?= 1
# Every header a program includes is a prerequisite as the compiler reports it
# while compiling, not as a list kept beside the rule: -MMD writes the user
# headers the source reached, -MF names the file, -MT names the target, and
# -MP adds an empty rule per header so a deleted one is not a missing
# prerequisite [source: GCC 15 manual, 3.13 Options Controlling the
# Preprocessor, -MMD -MP -MF -MT].
DEPFLAGS = -MMD -MP -MF $@.d -MT $@

# The twins mirror the corpus's own layout, chapter/section/name, so discovery
# walks the tree rather than one level of it.
TWINS_DIR := language-feature-examples
HANDWRITTEN := basics data gallery integration live operations reasoning
SOURCES := $(shell find $(HANDWRITTEN) $(TWINS_DIR) -name '*.c' \
             -not -path '*/_fixtures/*' 2>/dev/null | LC_ALL=C sort)
PROGRAMS := $(patsubst %.c,build/%,$(SOURCES))
TWIN_PROGRAMS := $(filter build/$(TWINS_DIR)/%,$(PROGRAMS))
HAND_PROGRAMS := $(filter-out $(TWIN_PROGRAMS),$(PROGRAMS))

SQLITE_CFLAGS ?= $(shell pkg-config --cflags sqlite3 2>/dev/null)
SQLITE_LIBS ?= $(shell pkg-config --libs sqlite3 2>/dev/null)
SQL_PROGRAMS := build/integration/sqlite_space build/integration/persistent_migration build/gallery/journaled_observed_store
# A program that links a C library beyond libc names that library's pkg-config
# packages, the one fact per program; the build rule below compiles and links
# them. A twin that holds a shipped library against the C library doing the
# same job is the usual case.
LIBRARY_TWINS := build/$(TWINS_DIR)/ch08-data/08-03-the-shipped-libraries
build/gallery/symbolic_tensors: PACKAGES = openblas
$(LIBRARY_TWINS)/04-regex_lib: PACKAGES = libpcre2-8
$(LIBRARY_TWINS)/05-json_lib: PACKAGES = libcjson
$(LIBRARY_TWINS)/06-crypto_lib: PACKAGES = libcrypto
$(LIBRARY_TWINS)/13-vector_lib: PACKAGES = gmp
$(LIBRARY_TWINS)/14-reflect_lib: PACKAGES = libcjson
$(LIBRARY_TWINS)/16-the_prolog_rung: PACKAGES = libpcre2-8 libcrypto
$(LIBRARY_TWINS)/18-string_lib: PACKAGES = libutf8proc
$(LIBRARY_TWINS)/22-functional_lib: PACKAGES = libutf8proc
$(LIBRARY_TWINS)/26-unicode_lib: PACKAGES = libutf8proc
$(LIBRARY_TWINS)/27-parsing_lib: PACKAGES = libutf8proc
$(LIBRARY_TWINS)/28-yaml_lib: PACKAGES = yaml-0.1
$(LIBRARY_TWINS)/29-markup_lib: PACKAGES = libxml-2.0
$(LIBRARY_TWINS)/30-encoding_lib: PACKAGES = libcrypto libutf8proc
$(LIBRARY_TWINS)/33-uuid_lib: PACKAGES = uuid libcrypto
$(LIBRARY_TWINS)/35-math_lib: PACKAGES = gmp
$(LIBRARY_TWINS)/36-random_lib: PACKAGES = gmp
$(LIBRARY_TWINS)/37-statistics_lib: PACKAGES = gmp
$(LIBRARY_TWINS)/38-http_lib: PACKAGES = libcurl
$(LIBRARY_TWINS)/39-uri_lib: PACKAGES = liburiparser libutf8proc
$(LIBRARY_TWINS)/41-compression_lib: PACKAGES = zlib libarchive
$(LIBRARY_TWINS)/42-database_lib: PACKAGES = gmp
$(LIBRARY_TWINS)/43-testing_lib: PACKAGES = gmp
$(LIBRARY_TWINS)/44-cli_lib: PACKAGES = gmp libutf8proc
$(LIBRARY_TWINS)/42-database_lib: CPPFLAGS += $(SQLITE_CFLAGS)
$(LIBRARY_TWINS)/42-database_lib: LDLIBS += $(SQLITE_LIBS)
PACKAGE_CFLAGS = $(if $(PACKAGES),$(patsubst -I%,-isystem %,$(shell pkg-config --cflags $(PACKAGES))))
PACKAGE_LIBS = $(if $(PACKAGES),$(shell pkg-config --libs $(PACKAGES)))
$(SQL_PROGRAMS): CPPFLAGS += $(SQLITE_CFLAGS)
$(SQL_PROGRAMS): LDLIBS += $(SQLITE_LIBS)
$(SQL_PROGRAMS): build/support/sqlite_store.o
build/support/sqlite_store.o: support/sqlite_store.c
	@mkdir -p $(@D)
	$(CC) $(DEPFLAGS) $(CPPFLAGS) $(SQLITE_CFLAGS) $(CFLAGS) -c $< -o $@

# Every program's translation unit as the compiler sees it, macros expanded,
# written with the flags that compile the program: a program's unit is reached
# only as its prerequisite, so it inherits the program's PACKAGES and
# CPPFLAGS. The twin lane reads the units for the doors that hand the engine
# MeTTa source, which a macro can spell where the source never names one
# (mt_lower expands to mt_do); common.c and lane.c are linked into every
# program and the support files into some, so theirs are read too.
SHARED_UNITS := $(patsubst %.c,build/%.i,$(wildcard support/*.c checks/*.c) common.c lane.c)
$(PROGRAMS): build/%: build/%.i
build/support/sqlite_store.i: CPPFLAGS += $(SQLITE_CFLAGS)
build/%.i: %.c
	@mkdir -p $(@D)
	$(CC) -E $(DEPFLAGS) $(CPPFLAGS) $(PACKAGE_CFLAGS) $(CFLAGS) $< -o $@

.PHONY: all check twins list check-helpers check-consumers index surface clean verification
all: $(PROGRAMS) $(SHARED_UNITS) build/tools/original

surface:
	ENGINE_PATH=$(CMETTA_ENGINE) sh $(CMETTA_DIR)/build.sh

build/integration/shared_extension: build/plugins/arithmetic.so
build/plugins/arithmetic.so: support/arithmetic_extension.c $(CMETTA_DIR)/libcmetta.so
	@mkdir -p $(@D)
	$(CC) $(DEPFLAGS) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< $(LDFLAGS) -lcmetta -o $@

check-consumers:
	$(MAKE) -C $(CMETTA_DIR) install ENGINE_PATH=$(CMETTA_ENGINE) PREFIX=$(abspath build/prefix)
	PKG_CONFIG_PATH=$(abspath build/prefix/lib/pkgconfig) $(MAKE) -C consumer check
	PKG_CONFIG_PATH=$(abspath build/prefix/lib/pkgconfig) cmake -S consumer -B build/consumer-cmake
	cmake --build build/consumer-cmake
	env -u METTA_PATH ctest --test-dir build/consumer-cmake --output-on-failure

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(DEPFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/tools/original: tools/original.c build/lane.o
	@mkdir -p $(@D)
	$(CC) $(DEPFLAGS) $(CPPFLAGS) $(CFLAGS) $< build/lane.o $(LDFLAGS) $(LDLIBS) -o $@

build/%: %.c build/common.o build/lane.o $(CMETTA_DIR)/libcmetta.so
	@mkdir -p $(@D)
	$(CC) $(DEPFLAGS) $(CPPFLAGS) $(PACKAGE_CFLAGS) $(CFLAGS) $< build/common.o build/lane.o $(filter build/support/%.o,$^) $(LDFLAGS) $(LDLIBS) $(PACKAGE_LIBS) -o $@

# The hand-written programs prove their own claims; the twins are run by the
# lane, beside their originals, so each is compared as well as run.
check: all check-helpers
	python3 tools/run.py --jobs $(JOBS) $(HAND_PROGRAMS)
	python3 tools/twin_lane.py --jobs $(JOBS) --engine $(CMETTA_ENGINE)
	python3 tools/twin_lane_selftest.py --engine $(CMETTA_ENGINE)
	python3 tools/index.py --engine $(CMETTA_ENGINE) --check
	python3 tools/verification_selftest.py
	python3 tools/verification.py --check

# The verification record's parts a run determines, written from one run's
# C seat gate output, GATE, and make output, LOG: the identity paragraph, the
# host block, the Results table, the receipts, and the gate's output with its
# paths repository-relative.
verification:
	python3 tools/verification.py --engine $(CMETTA_ENGINE) $(if $(GATE),--gate $(GATE)) $(if $(LOG),--make $(LOG))

twins: all
	python3 tools/twin_lane.py --jobs $(JOBS) --engine $(CMETTA_ENGINE) $(TWIN)

index:
	python3 tools/index.py --engine $(CMETTA_ENGINE)

check-helpers: build/helper-check
	@set -e; for mode in false empty; do \
	  if ./build/helper-check $$mode >build/helper-$$mode.log 2>&1; then \
	    cat build/helper-$$mode.log; exit 1; \
	  fi; grep -q '^FAIL ' build/helper-$$mode.log; \
	done
build/helper-check: checks/helpers.c common.c lane.c common.h lane.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DNDEBUG checks/helpers.c common.c lane.c $(LDFLAGS) $(LDLIBS) -o $@

list:
	@printf '%s\n' $(SOURCES)

clean:
	rm -rf build

-include $(shell find build -name '*.d' 2>/dev/null)
