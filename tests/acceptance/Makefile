CFLAGS=-Wall -Werror -g -I../include -I../../include -I ../../src -m32 -pthread
LDFLAGS=
LIBS=../../src/libinstruments.so
ifeq ($(shell uname), Darwin)
LDFLAGS+=-Wl,-flat_namespace,-undefined,dynamic_lookup
endif

TESTS=test_coin_flip.o external_estimator_test.o test_chooser_arg.o

all: run_tests

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

run_tests: main.o $(TESTS)
	gcc $(LDFLAGS) main.o $(TESTS) -o $@ $(LIBS)

clean:
	rm -f run_tests *.o
