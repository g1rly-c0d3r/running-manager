CC=clang
cc_warnings=-Wall -Werror -Wpedantic -Wextra -Wshadow -Wconversion -pedantic-errors
cc_optimizations=-O3 -march=native

rund: server/rund.c 
	${CC} ${cc_warnings} ${cc_optimizations} -o $@ $^

.PHONY: debug
debug: server/rund.c 
	${CC} -g  -o run-db $^ 
	gdb run-db

.PHONY:clean
clean:
	rm -f run-db rund
