LIBRARIES := libinstruments.so
OBJS := \
	estimator.o \
	estimator_registry.o \
	instruments.o \
	redundant_strategy.o \
	running_mean_estimator.o \
	simple_stats_expectation_evaluator.o \
	strategy.o \
	strategy_evaluator.o \
	trusted_oracle_strategy_evaluator.o

CXXFLAGS := -g -Wall -Werror -I. -I../include -fpic

all: $(LIBRARIES)

libinstruments.so: $(OBJS)
	g++ $(LDFLAGS) -shared -o $@ $^

clean:
	rm -f $(OBJS) $(LIBRARIES) *~

include deps.mk
