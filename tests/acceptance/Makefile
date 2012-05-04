CFLAGS=-Wall -Werror -g -I../include -I../../include -m32
LDFLAGS=
LIBS=../../src/libinstruments.so
ifeq ($(shell uname), Darwin)
LDFLAGS+=-Wl,-flat_namespace,-undefined,dynamic_lookup
endif

TESTS=test_coin_flip.o

all: run_tests

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

run_tests: main.o $(TESTS)
	gcc $(LDFLAGS) main.o $(TESTS) -o $@ $(LIBS)

clean:
	rm -f run_tests *.o
