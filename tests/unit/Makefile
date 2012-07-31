EXECUTABLES=run_unit_tests
all: $(EXECUTABLES) ../../common/R_generated_vars.mk

../../common/R_generated_vars.mk: ../../common/Makefile
	make -C ../../common

include ../../common/R_generated_vars.mk

R_CXXFLAGS := $(subst -O3,,$(R_CXXFLAGS))

CXXFLAGS += -Wall -Werror -I. -I.. -I../../include -I../../src -m32 -g $(R_CXXFLAGS)
LDFLAGS += -L. -lcppunit $(R_LDLIBS)

ifeq ($(shell uname), Darwin)
CXXFLAGS += -I/opt/local/include
LDFLAGS += -L/opt/local/lib
else
CXXFLAGS += -pthread
endif


%.o: %.cc %.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: ../../src/%.cc ../../src/%.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: ../%.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: ../../src/%.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

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
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f *.o *~ $(EXECUTABLES)

include ../../deps.mk
