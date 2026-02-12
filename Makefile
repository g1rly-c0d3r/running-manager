CC=clang
cc_warnings=-Wall -Werror -Wpedantic -Wextra -Wshadow -Wconversion -pedantic-errors
cc_optimizations?=-O3 -march=native -mtune=native
CCFLAGS+=$(cc_warnings) $(cc_optimizations)

INSTALL_DIR ?= /usr/bin/

srcs=$(wildcard server/*.c)

rnmnd: $(srcs)
	${CC} ${cc_warnings} ${cc_optimizations} -o $@ server/main.c -larena

tests: test/test.c 
	$(CC) $(cc_warnings) $(cc_optimizations) -o $@ $< -larena

.PHONY: debug clean install
debug: server/main.c 
	${CC} -g -o run-db $^ -larena

clean:
	rm -f run-db rnmnd run.out err.out README.pdf tests

install: rnmnd
	cp rnmnd $(INSTALL_DIR)
	cp ./client/rnmn $(INSTALL_DIR)
