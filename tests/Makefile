# Host tests for the portable core (no devkitPro needed): `make -C tests`
CC ?= cc
CFLAGS ?= -std=c11 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -I../source/core

test_core: test_core.c ../source/core/*.c
	$(CC) $(CFLAGS) test_core.c ../source/core/*.c -lm -o $@

.PHONY: test clean
test: test_core
	./test_core

clean:
	rm -f test_core
