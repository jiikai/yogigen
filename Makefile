OS := $(shell uname -s)
CC := gcc
EXT := c
BINDIR := bin
SRCDIR := src
LOGDIR := log
LIBDIR := lib
TESTDIR := test
PREFIX := /usr/local
INCLUDE := -I$(PREFIX)/include -I/usr/include/postgresql -Isrc
LIBINCLUDE := -L$(PREFIX)/lib -L/postgresql
STD := -std=c99 -pedantic
STACK := -fstack-protector -Wstack-protector
# ^^or: -fstack-protector-all (extra protection)
WARNS := -Wall -Wextra
DEBUG := -g3
CFLAGS := -O3 -pthread -rdynamic $(INCLUDE) $(STD) $(STACK) $(WARNS) $(OPTFLAGS)
LIBS := -ldl -lrt -lpthread -lpq -lcrypto -lm $(LIBINCLUDE) $(OPTLIBS)
TESTLIBS := $(LIBS) -lcurl -L/curl
SRCS := $(wildcard $(SRCDIR)/**/*.c $(SRCDIR)/*.c)
TEST_SRCS= $(wildcard $(TESTDIR)/*_test.c)
OBJECTS :=$(patsubst %.c,$(LIBDIR)/%.o,$(SRCS))
TEST_OBJECTS :=$(patsubst %.c,%.o,$(TEST_SRCS))
BINARY := main

# RULES

.PHONY: all valgrind tests clean

default: all

all: $(BINDIR)
	$(CC) $(CFLAGS) $(DEBUG) $(SRCS) -DHEROKU -o $(BINDIR)/$(BINARY) $(LIBS)

$(BINDIR):
	mkdir $@

valgrind:
	valgrind \
		--track-origins=yes \
		--leak-check=full \
		--leak-resolution=high \
		--show-leak-kinds=all \
		--log-file=$(LOGDIR)/$@-valgrind.log \
		$(BINDIR)/$(BINARY)
	@echo -en "\n- - - Log file: $(LOGDIR)/$@-valgrind.log - - -\n"


# Compile tests and run the test binary

tests:
	$(CC) $(CFLAGS) -I/curl $(DEBUG) $(TEST_SRCS) -o $(TEST_OBJECTS) $(TEST_LIBS)
	@which ldconfig && ldconfig -C /tmp/ld.so.cache || true # caching the library linking
	@sh /$(TESTDIR)/runtests.sh

# Rule for cleaning the project
clean:
	@rm -rvf $(BINDIR)/* $(LIBDIR)/* $(LOGDIR)/*;
	@find . -name "*.gc*" -exec rm {} \;
ifeq ($(OS),Darwin)
	@rm -rf `find . -name "*.dSYM" -print`
endif
