LIBRARIES := libinstruments.so
all: $(LIBRARIES)

../common/R_generated_vars.mk: ../common/Makefile
	make -C ../common

include ../common/R_generated_vars.mk

OBJS := \
	abstract_joint_distribution.o \
	empirical_error_strategy_evaluator.o \
	estimator.o \
	estimator_registry.o \
	eval_method.o \
	external_estimator.o \
	goal_adaptive_resource_weight.o \
	instruments.o \
	joint_distribution.o \
	joint_distributions/intnw_joint_distribution.o \
	r_singleton.o \
	resource_weights.o \
	running_mean_estimator.o \
	stats_distribution.o \
	stats_distribution_all_samples.o \
	stats_distribution_binned.o \
	strategy.o \
	strategy_evaluator.o \
	timeops.o \
	trusted_oracle_strategy_evaluator.o


CXXFLAGS := -g -Wall -Werror -I. -I../include -fpic -pthread $(R_CXXFLAGS)
#CXXFLAGS := $(subst -O3,-O0,$(CXXFLAGS))
#CXXFLAGS += -fno-inline
CXXFLAGS += -O3 -DNDEBUG
LDFLAGS := -lpthread $(R_LDLIBS) -Wl,--no-undefined

ifeq ($(shell uname), Darwin)
CXXFLAGS += -I/opt/local/include
endif

libinstruments.so: $(OBJS)
	g++  -shared -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(LIBRARIES) *~

include ../deps.mk
