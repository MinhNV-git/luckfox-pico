ifeq ($(APP_PARAM), )
    APP_PARAM:=../Makefile.param
    include $(APP_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)

PKG_NAME := drone_mini
PKG_BIN ?= out

SRC := $(wildcard *.c) $(wildcard imu/*.c) $(wildcard tools/*.c)
CFLAGS := -I include -pthread -lm

all:
	rm -rf $(PKG_BIN); \
	mkdir -p $(PKG_BIN)/bin;
	$(RK_APP_CROSS)-gcc $(CFLAGS) $(SRC) -o $(PKG_BIN)/bin/drone_mini
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT), $(PKG_BIN))
	@echo "build $(PKG_NAME) done"

clean:
	@rm -rf $(PKG_BIN)

distclean: clean

info:
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
	@echo -e "Drone Mini App"
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
