CFLAGS=-Wall -Werror -g -I../include -m32
LDFLAGS=-L../src -linstruments
ifeq ($(shell uname), Darwin)
LDFLAGS+=-Wl,-flat_namespace,-undefined,dynamic_lookup
endif

TESTS=test_coin_flip.o

all: run_tests

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

run_tests: main.o ctest.h $(TESTS)
	gcc $(LDFLAGS) main.o $(TESTS) -o $@

clean:
	rm -f run_tests *.o
