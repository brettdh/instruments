include ../../src/R.mk

CXXFLAGS += -Wall -Werror -I. -I.. -I../../include -I../../src -m32 -g -pthread $(R_CXXFLAGS)
LDFLAGS += -L. -lcppunit $(R_LDLIBS)

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

TESTOBJS := \
	r_test.o \
	stats_distribution_test.o \
	empirical_error_strategy_evaluator_test.o \
	goal_adaptive_resource_weight_test.o \
	running_mean_estimator_test.o \
	strategy_estimators_discovery_test.o

SUPPORTOBJS := \
	estimator.o \
	estimator_registry.o \
	external_estimator.o \
	goal_adaptive_resource_weight.o \
	instruments.o \
	r_singleton.o \
	running_mean_estimator.o \
	strategy.o \
	strategy_evaluator.o \
	strategy_evaluation_context.o \
	empirical_error_strategy_evaluator.o \
	stats_distribution.o \
	stats_distribution_all_samples.o \
	stats_distribution_binned.o \
	timeops.o \
	trusted_oracle_strategy_evaluator.o

run_unit_tests: run_all_tests.o $(TESTOBJS) $(SUPPORTOBJS)
	g++ -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f *.o *~ $(EXECUTABLES)

include ../../deps.mk
