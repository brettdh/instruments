LIBRARIES := libinstruments.so
OBJS := \
	empirical_error_strategy_evaluator.o \
	estimator.o \
	estimator_registry.o \
	goal_adaptive_resource_weight.o \
	instruments.o \
	running_mean_estimator.o \
	stats_distribution.o \
	stats_distribution_all_samples.o \
	strategy.o \
	strategy_evaluator.o \
	timeops.o \
	trusted_oracle_strategy_evaluator.o

CXXFLAGS := -g -Wall -Werror -I. -I../include -fpic -pthread
LDFLAGS := -lpthread

all: $(LIBRARIES)

libinstruments.so: $(OBJS)
	g++ $(LDFLAGS) -shared -o $@ $^

clean:
	rm -f $(OBJS) $(LIBRARIES) *~

include ../deps.mk
