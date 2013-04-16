CFLAGS=-Wall -Werror -g -I../include -I../../include -I../../src -m32 -pthread
LDFLAGS=
DEBUG_LIBS=../../src/libinstruments_debug.so
LIBS=../../src/libinstruments.so

TESTS=test_coin_flip.o external_estimator_test.o test_chooser_arg.o intnw_specific_test.o
BINS=run_tests run_perf_test

all: $(BINS)

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

run_tests: main.o $(TESTS)
	gcc $(LDFLAGS) main.o $(TESTS) -o $@ $(DEBUG_LIBS)

performance_test.o: performance_test.c
	gcc $(CFLAGS) -c -o $@ $<

run_perf_test: performance_test.o
	gcc $(LDFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f $(BINS) *.o
