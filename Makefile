# Purpose: discover, compile and execute every standalone C example.
# Assumes: CMETTA_DIR contains a built CMeTTa surface and its public header.
# Guarantees: check stops on a failed program [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
# Open Obligations: None.
CMETTA_DIR ?= $(abspath ../PeTTa/extensions/cmetta)
CMETTA_ENGINE ?= $(abspath $(CMETTA_DIR)/../..)
.DEFAULT_GOAL := all
CC ?= cc
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -I. -I$(CMETTA_DIR)
LDFLAGS += -L$(CMETTA_DIR) -Wl,-rpath,$(CMETTA_DIR)
LDLIBS += -lcmetta -lm -pthread -ldl
CATEGORIES := basics data gallery integration language-feature-examples live operations reasoning
JOBS ?= 1
SOURCES := $(sort $(foreach dir,$(CATEGORIES),$(wildcard $(dir)/*.c)))
PROGRAMS := $(patsubst %.c,build/%,$(SOURCES))
SQLITE_CFLAGS ?= $(shell pkg-config --cflags sqlite3 2>/dev/null)
SQLITE_LIBS ?= $(shell pkg-config --libs sqlite3 2>/dev/null)
SQL_PROGRAMS := build/integration/sqlite_space build/integration/persistent_migration build/gallery/journaled_observed_store
build/gallery/symbolic_tensors: CPPFLAGS += $(patsubst -I%,-isystem %,$(shell pkg-config --cflags openblas))
build/gallery/symbolic_tensors: LDLIBS += $(shell pkg-config --libs openblas)
$(SQL_PROGRAMS): CPPFLAGS += $(SQLITE_CFLAGS)
$(SQL_PROGRAMS): LDLIBS += $(SQLITE_LIBS)
$(SQL_PROGRAMS): build/support/sqlite_store.o
build/support/sqlite_store.o: support/sqlite_store.c support/sqlite_store.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(SQLITE_CFLAGS) $(CFLAGS) -c $< -o $@
.PHONY: all check list check-helpers check-consumers generated-check surface clean
all: $(PROGRAMS)
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
build/common.o: common.c common.h $(CMETTA_DIR)/cmetta.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/%: %.c common.h build/common.o $(CMETTA_DIR)/libcmetta.so
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< build/common.o $(filter build/support/%.o,$^) $(LDFLAGS) $(LDLIBS) -o $@
check: all check-helpers generated-check
	python3 tools/run.py --jobs $(JOBS) $(PROGRAMS)
generated-check:
	python3 tools/generate.py --check
	python3 tools/index.py --check
check-helpers: build/helper-check
	@set -e; for mode in false empty; do \
	  if ./build/helper-check $$mode >build/helper-$$mode.log 2>&1; then \
	    cat build/helper-$$mode.log; exit 1; \
	  fi; grep -q '^FAIL ' build/helper-$$mode.log; \
	done
build/helper-check: checks/helpers.c common.c common.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DNDEBUG checks/helpers.c common.c $(LDFLAGS) $(LDLIBS) -o $@
list:
	@printf '%s\n' $(SOURCES)
clean:
	rm -rf build
