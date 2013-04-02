LIBRARIES := libinstruments.so
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
	evaluators/confidence_bounds_strategy_evaluator.o \
	evaluators/empirical_error_strategy_evaluator.o \
	evaluators/trusted_oracle_strategy_evaluator.o \
	external_estimator.o \
	goal_adaptive_resource_weight.o \
	instruments.o \
	generic_joint_distribution.o \
	joint_distributions/intnw_joint_distribution.o \
	r_singleton.o \
	resource_weights.o \
	running_mean_estimator.o \
	stats_distribution.o \
	stats_distribution_all_samples.o \
	stats_distribution_binned.o \
	strategy.o \
	strategy_evaluator.o \
	timeops.o


CXXFLAGS := -g -Wall -Werror -I. -I./evaluators -I./joint_distributions -I../include -fpic -pthread $(R_CXXFLAGS)
#CXXFLAGS := $(subst -O3,-O0,$(CXXFLAGS))
#CXXFLAGS += -fno-inline
CXXFLAGS += -O3 -DNDEBUG # -std=gnu++0x
LDFLAGS := -lpthread $(R_LDLIBS) -L/usr/local/lib -lmocktime -Wl,--no-undefined

ifeq ($(shell uname), Darwin)
CXXFLAGS += -I/opt/local/include
endif

LIB := libinstruments.so
HEADERS := ../include/instruments.h

$(LIB): $(OBJS)
	g++  -shared -o $@ $^ $(LDFLAGS)

.libinstall: $(LIB)
	install $(LIB) /usr/local/lib/
	-touch $@

.hdrinstall: $(HEADERS)
	install $(HEADERS) /usr/local/include/
	-touch $@

install: .libinstall .hdrinstall

clean:
	rm -f $(OBJS) $(LIBRARIES) *~

include ../deps.mk

check-syntax:
	g++ -o /dev/null $(OBJS:%.o=%.cc) $(CXXFLAGS)
