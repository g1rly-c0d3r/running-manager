CC=clang
cc_warnings=-Wall -Werror -Wpedantic -Wextra -Wshadow -Wconversion -pedantic-errors
cc_optimizations=-O3 -march=native

OBJS := ./server/main.c ./server/rund.c ./server/queue.c ./server/watch.c ./server/run_sim.c
INSTALL_DIR ?= /usr/bin/

rnmnd: $(OBJS)
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
