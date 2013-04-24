LIBRARIES := libinstruments.so libinstruments_debug.so
all: $(LIBRARIES)

../common/R_generated_vars.mk: ../common/Makefile
	make -C ../common

include ../common/R_generated_vars.mk

OBJS := \
	abstract_joint_distribution.o \
	debug.o \
	error_calculation.o \
	estimator.o \
	estimator_registry.o \
	eval_method.o \
	evaluators/bayesian_strategy_evaluator.o \
	evaluators/confidence_bounds_strategy_evaluator.o \
	evaluators/empirical_error_strategy_evaluator.o \
	evaluators/trusted_oracle_strategy_evaluator.o \
	evaluators/students_t.o \
	external_estimator.o \
	goal_adaptive_resource_weight.o \
	instruments.o \
	generic_joint_distribution.o \
	joint_distributions/intnw_joint_distribution.o \
	joint_distributions/bayesian_intnw_posterior_distribution.o \
	r_singleton.o \
	resource_weights.o \
	running_mean_estimator.o \
	stats_distribution.o \
	stats_distribution_all_samples.o \
	stats_distribution_binned.o \
	strategy.o \
	strategy_evaluator.o \
	timeops.o

DEBUG_OBJS := $(OBJS:%.o=%_debug.o)

CXXFLAGS := -g -Wall -Werror -I. -I./evaluators -I./joint_distributions -I../include -fpic -pthread $(R_CXXFLAGS)
LDFLAGS := -lpthread $(R_LDLIBS) -L/usr/local/lib -lmocktime -Wl,--no-undefined

DEBUG_CXXFLAGS := $(subst -O3,-O0,$(CXXFLAGS)) 
OPTI_FLAGS += -O3 -DNDEBUG

ifeq ($(shell uname), Darwin)
CXXFLAGS += -I/opt/local/include
endif

LIB := libinstruments.so
LIB_DEBUG := libinstruments_debug.so
HEADERS := ../include/instruments.h

%.o: %.cc
	g++ -o $@ -c $< $(CXXFLAGS) $(OPTI_FLAGS)

%_debug.o: %.cc
	g++ -o $@ -c $< $(DEBUG_CXXFLAGS)

$(LIB): $(OBJS)
	g++  -shared -o $@ $^ $(LDFLAGS) $(OPTI_FLAGS)

$(LIB_DEBUG): $(DEBUG_OBJS)
	g++ -shared -o $@ $^ $(LDFLAGS)

.libinstall: $(LIB)
	install $(LIB) /usr/local/lib/
	-touch $@

.hdrinstall: $(HEADERS)
	install $(HEADERS) /usr/local/include/
	-touch $@

install: .libinstall .hdrinstall

clean:
	rm -f $(OBJS) $(DEBUG_OBJS) $(LIBRARIES) *~

include ../deps.mk

check-syntax:
	g++ -o /dev/null $(OBJS:%.o=%.cc) $(CXXFLAGS)
