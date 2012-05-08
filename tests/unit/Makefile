CXXFLAGS += -Wall -Werror -I. -I.. -I../../src -m32 -g
LDFLAGS += -L. -lcppunit

EXECUTABLES=run_unit_tests

all: $(EXECUTABLES)

%.o: %.cc %.h
	g++ $(CXXFLAGS) -c -o $@ $<

%.o: ../../src/%.cc ../../src/%.h
	g++ $(CXXFLAGS) -c -o $@ $<

%.o: %.cc
	g++ $(CXXFLAGS) -c -o $@ $<

%.o: ../%.cc
	g++ $(CXXFLAGS) -c -o $@ $<

%.o: ../../src/%.cc
	g++ $(CXXFLAGS) -c -o $@ $<

TESTOBJS := stats_distribution_test.o
SUPPORTOBJS := stats_distribution.o stats_distribution_all_samples.o

run_unit_tests: run_all_tests.o $(TESTOBJS) $(SUPPORTOBJS)
	g++ -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f *.o *~ $(EXECUTABLES)

include ../../deps.mk
