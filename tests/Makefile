CFLAGS=-Wall -Werror -g -I../include

TESTS=test_fair_coin.c

all: run_tests

run_tests: main.c ctest.h $(TESTS)
	gcc $(CFLAGS) main.c $(TESTS) -o $@

clean:
	rm -f run_tests
