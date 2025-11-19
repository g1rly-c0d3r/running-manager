CC=clang
cc_warnings=-Wall -Werror -Wpedantic -Wextra -Wshadow -Wconversion -pedantic-errors
cc_optimizations=-O3 -march=native

OBJS := ./server/main.c ./server/rund.c ./server/queue.c ./server/watch.c ./server/run_sim.c

rnmnd: $(OBJS)
	${CC} ${cc_warnings} ${cc_optimizations} -o $@ server/main.c -larena

.PHONY: debug
debug: server/main.c 
	${CC} -g -fsanitize=address -o run-db $^ -larena
	gdb run-db

.PHONY:clean
clean:
	rm -f run-db rnmnd
