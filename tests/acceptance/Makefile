CFLAGS=-Wall -Werror -g -I../include -I../../include -I ../../src -m32 -pthread
LDFLAGS=
LIBS=../../src/libinstruments.so

TESTS=test_coin_flip.o external_estimator_test.o test_chooser_arg.o intnw_specific_test.o

all: run_tests run_perf_test

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

run_tests: main.o $(TESTS)
	gcc $(LDFLAGS) main.o $(TESTS) -o $@ $(LIBS)

run_perf_test: brute_force_performance_test.o
	gcc $(LDFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f run_tests *.o
