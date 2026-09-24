# Purpose: build shared and archive consumers using installed pkg-config metadata.
# Assumes: PKG_CONFIG_PATH locates cmetta.pc; the archive keeps SWI dynamically linked.
# Guarantees: check runs both programs [tested: make check-consumers; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
ROOT := $(abspath ..)
OUT ?= $(ROOT)/build/consumer-make
CMETTA_LIBDIR := $(shell pkg-config --variable=libdir cmetta)
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -I$(ROOT) $(shell pkg-config --cflags cmetta)
SOURCE := $(ROOT)/integration/installed_consumer.c $(ROOT)/common.c $(ROOT)/lane.c
.PHONY: all check
all: $(OUT)/shared $(OUT)/archive
$(OUT)/shared: $(SOURCE) $(ROOT)/common.h $(ROOT)/lane.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCE) $(shell pkg-config --libs cmetta) -Wl,-rpath,$(CMETTA_LIBDIR) -o $@
$(OUT)/archive: $(SOURCE) $(ROOT)/common.h $(ROOT)/lane.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCE) $(CMETTA_LIBDIR)/libcmetta.a $(shell pkg-config --static --libs cmetta) -o $@
check: all
	env -u METTA_PATH $(OUT)/shared
	env -u METTA_PATH $(OUT)/archive
	@if readelf -dW $(OUT)/archive | grep -q 'Shared library: \[libcmetta'; then exit 1; fi
