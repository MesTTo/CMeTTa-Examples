# Purpose: discover, compile and execute every standalone C example.
# Assumes: CMETTA_DIR contains a built CMeTTa surface and its public header.
# Guarantees: check stops on a failed program [tested: make check; commit=WORKTREE].
# Open Obligations: None.
CMETTA_DIR ?= $(abspath ../PeTTa/extensions/cmetta)
CC ?= cc
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -I. -I$(CMETTA_DIR)
LDFLAGS += -L$(CMETTA_DIR) -Wl,-rpath,$(CMETTA_DIR)
LDLIBS += -lcmetta -lm -pthread -ldl
CATEGORIES := basics data gallery integration language-feature-examples live operations reasoning
SOURCES := $(sort $(foreach dir,$(CATEGORIES),$(wildcard $(dir)/*.c)))
PROGRAMS := $(patsubst %.c,build/%,$(SOURCES))
.PHONY: all check list check-helpers clean
all: $(PROGRAMS)
build/common.o: common.c common.h $(CMETTA_DIR)/cmetta.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/%: %.c common.h build/common.o $(CMETTA_DIR)/libcmetta.so
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< build/common.o $(LDFLAGS) $(LDLIBS) -o $@
check: all check-helpers
	@set -e; mkdir -p build/logs; for example in $(PROGRAMS); do \
	  log=build/logs/$$(printf '%s' "$$example" | tr / _).log; \
	  if "./$$example" >"$$log" 2>&1; then cat "$$log"; \
	  else cat "$$log"; exit 1; fi; \
	done
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
