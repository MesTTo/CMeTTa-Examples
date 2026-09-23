# Purpose: discover, compile and run every C example, and hold each language
#   twin against the MeTTa original it mirrors.
# Assumes: CMETTA_DIR holds a built CMeTTa surface and its header, and
#   CMETTA_ENGINE the engine tree whose examples/ are the originals.
# Guarantees: check stops on a failed program, a failed claim, or a twin that
#   disagrees with its original [tested: make check; commit=WORKTREE].
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

# The twins mirror the corpus's own layout, chapter/section/name, so discovery
# walks the tree rather than one level of it.
TWINS_DIR := language-feature-examples
HANDWRITTEN := basics data gallery integration live operations reasoning
SOURCES := $(shell find $(HANDWRITTEN) $(TWINS_DIR) -name '*.c' \
             -not -path '*/_fixtures/*' 2>/dev/null | LC_ALL=C sort)
PROGRAMS := $(patsubst %.c,build/%,$(SOURCES))
TWIN_PROGRAMS := $(filter build/$(TWINS_DIR)/%,$(PROGRAMS))
HAND_PROGRAMS := $(filter-out $(TWIN_PROGRAMS),$(PROGRAMS))
# A twin may include a header shared with its neighbours, as an import is.
TWIN_HEADERS := $(shell find $(TWINS_DIR) -name '*.h' 2>/dev/null)
$(TWIN_PROGRAMS): $(TWIN_HEADERS)

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
PACKAGE_CFLAGS = $(if $(PACKAGES),$(patsubst -I%,-isystem %,$(shell pkg-config --cflags $(PACKAGES))))
PACKAGE_LIBS = $(if $(PACKAGES),$(shell pkg-config --libs $(PACKAGES)))
$(SQL_PROGRAMS): CPPFLAGS += $(SQLITE_CFLAGS)
$(SQL_PROGRAMS): LDLIBS += $(SQLITE_LIBS)
$(SQL_PROGRAMS): build/support/sqlite_store.o
build/support/sqlite_store.o: support/sqlite_store.c support/sqlite_store.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(SQLITE_CFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: all check twins list check-helpers check-consumers index surface clean
all: $(PROGRAMS) build/tools/original

surface:
	ENGINE_PATH=$(CMETTA_ENGINE) sh $(CMETTA_DIR)/build.sh

build/integration/shared_extension: build/plugins/arithmetic.so
build/plugins/arithmetic.so: support/arithmetic_extension.c $(CMETTA_DIR)/libcmetta.so
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< $(LDFLAGS) -lcmetta -o $@

check-consumers:
	$(MAKE) -C $(CMETTA_DIR) install ENGINE_PATH=$(CMETTA_ENGINE) PREFIX=$(abspath build/prefix)
	PKG_CONFIG_PATH=$(abspath build/prefix/lib/pkgconfig) $(MAKE) -C consumer check
	PKG_CONFIG_PATH=$(abspath build/prefix/lib/pkgconfig) cmake -S consumer -B build/consumer-cmake
	cmake --build build/consumer-cmake
	env -u METTA_PATH ctest --test-dir build/consumer-cmake --output-on-failure

build/%.o: %.c common.h lane.h $(CMETTA_DIR)/cmetta.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/tools/original: tools/original.c build/lane.o
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< build/lane.o $(LDFLAGS) $(LDLIBS) -o $@

build/%: %.c common.h lane.h build/common.o build/lane.o $(CMETTA_DIR)/libcmetta.so
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PACKAGE_CFLAGS) $(CFLAGS) $< build/common.o build/lane.o $(filter build/support/%.o,$^) $(LDFLAGS) $(LDLIBS) $(PACKAGE_LIBS) -o $@

# The hand-written programs prove their own claims; the twins are run by the
# lane, beside their originals, so each is compared as well as run.
check: all check-helpers
	python3 tools/run.py --jobs $(JOBS) $(HAND_PROGRAMS)
	python3 tools/twin_lane.py --jobs $(JOBS) --engine $(CMETTA_ENGINE)
	python3 tools/twin_lane_selftest.py --engine $(CMETTA_ENGINE)
	python3 tools/index.py --engine $(CMETTA_ENGINE) --check

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
