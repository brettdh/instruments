EXECUTABLES=run_unit_tests multi_array_perf_test multi_array_perf_test_deeper
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



DEP_SRCS=$(wildcard *.cc)
SUPPORT_DEP_SRCS=$(wildcard ../../src/*.cc)
DEPS=$(DEP_SRCS:%.cc=.%.dep) $(SUPPORT_DEP_SRCS:../../src/%.cc=.%.dep)

.%.dep: %.cc %.h
	$(CXX) -MM $(CXXFLAGS) $< >$@

.%.dep: ../../src/%.cc ../../src/%.h
	$(CXX) -MM $(CXXFLAGS) $< >$@

.%.dep: %.cc
	$(CXX) -MM $(CXXFLAGS) $< >$@

.%.dep: ../../src/%.cc
	$(CXX) -MM $(CXXFLAGS) $< >$@

include $(DEPS)


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
	empirical_error_strategy_evaluator_test.o \
	r_test.o \
	stats_distribution_test.o \
	goal_adaptive_resource_weight_test.o \
	multi_dimension_array_test.o \
	running_mean_estimator_test.o \
	strategy_estimators_discovery_test.o

SUPPORTOBJS := \
	empirical_error_strategy_evaluator.o \
	estimator.o \
	estimator_registry.o \
	external_estimator.o \
	goal_adaptive_resource_weight.o \
	instruments.o \
	joint_distribution.o \
	r_singleton.o \
	running_mean_estimator.o \
	strategy.o \
	strategy_evaluator.o \
	stats_distribution.o \
	stats_distribution_all_samples.o \
	stats_distribution_binned.o \
	timeops.o \
	trusted_oracle_strategy_evaluator.o

run_unit_tests: run_all_tests.o $(TESTOBJS) $(SUPPORTOBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

#OPTI_FLAGS := -march=native -mtune-native -ffast-math

multi_array_perf_test: multi_array_perf_test.o
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) -DNDEBUG -O3 $(OPTI_FLAGS)

multi_array_perf_test_deeper: multi_array_perf_test_deeper.o
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) -DNDEBUG -O3 $(OPTI_FLAGS)

clean:
	rm -f *.o *~ $(EXECUTABLES)

