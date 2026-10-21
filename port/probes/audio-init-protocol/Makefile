.DEFAULT_GOAL := all
XBE_TITLE = nxdk\ audio\ initialization\ protocol
NXDK_DIR ?= $(CURDIR)/../../../../../nxdk
NXDK_SDL = y
SRCS = $(CURDIR)/main.c $(CURDIR)/hal-close.c
CFLAGS += -O2 -DNDEBUG
.PHONY: verify-source
verify-source:
	python3 $(CURDIR)/verify-source.py
$(CURDIR)/hal-close.obj: verify-source
include $(NXDK_DIR)/Makefile
