CFLAGS=-Wall -Werror -g -I../include -I../../include -I../../src -m32 -pthread
LDFLAGS=
LIBS=../../src/libinstruments.so

TESTS=test_coin_flip.o external_estimator_test.o test_chooser_arg.o intnw_specific_test.o
BINS=run_tests run_brute_force_perf_test run_prob_perf_test

all: $(BINS)

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

run_tests: main.o $(TESTS)
	gcc $(LDFLAGS) main.o $(TESTS) -o $@ $(LIBS)

performance_test_brute_force.o: performance_test.c
	gcc $(CFLAGS) -DBRUTE_FORCE -c -o $@ $<

performance_test_prob.o: performance_test.c
	gcc $(CFLAGS) -DPROB_BOUNDS -c -o $@ $<

run_brute_force_perf_test: performance_test_brute_force.o
	gcc $(LDFLAGS) $^ -o $@ $(LIBS)

run_prob_perf_test: performance_test_prob.o
	gcc $(LDFLAGS) $^ -o $@ $(LIBS)

clean:
	rm -f $(BINS) *.o
