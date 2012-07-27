include R.mk

LIBRARIES := libinstruments.so
OBJS := \
	empirical_error_strategy_evaluator.o \
	estimator.o \
	estimator_registry.o \
	external_estimator.o \
	goal_adaptive_resource_weight.o \
	instruments.o \
	r_singleton.o \
	running_mean_estimator.o \
	stats_distribution.o \
	stats_distribution_all_samples.o \
	stats_distribution_binned.o \
	strategy.o \
	strategy_evaluator.o \
	strategy_evaluation_context.o \
	timeops.o \
	trusted_oracle_strategy_evaluator.o

CXXFLAGS := -g -Wall -Werror -I. -I../include -fpic -pthread $(R_CXXFLAGS)
LDFLAGS := -lpthread $(R_LDLIBS)

all: $(LIBRARIES)

libinstruments.so: $(OBJS)
	g++ $(LDFLAGS) -shared -o $@ $^

clean:
	rm -f $(OBJS) $(LIBRARIES) *~

include ../deps.mk
