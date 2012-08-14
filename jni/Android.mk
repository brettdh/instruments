LOCAL_PATH := $(call my-dir)

my_PROFILING_BUILD := yes
my_PROFILING_CFLAGS := -pg -fno-omit-frame-pointer -fno-function-sections -DPROFILING_BUILD

ifneq ($(my_PROFILING_BUILD),)
-include $(LOCAL_PATH)/android-ndk-profiler.mk
endif

common_CXXFLAGS:=-g -O3 -Wall -Werror -DANDROID
INSTRUMENTS_INCLUDES := \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../src

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION:=.cc
LOCAL_MODULE := instruments
LOCAL_SRC_FILES := $(addprefix ../src/, \
	abstract_joint_distribution.cc \
	empirical_error_strategy_evaluator.cc \
	estimator.cc \
	estimator_registry.cc \
	eval_method.cc \
	external_estimator.cc \
	goal_adaptive_resource_weight.cc \
	instruments.cc \
	joint_distribution.cc \
	joint_distributions/intnw_joint_distribution.cc \
	running_mean_estimator.cc \
	stats_distribution.cc \
	stats_distribution_all_samples.cc \
	strategy.cc \
	strategy_evaluator.cc \
	timeops.cc \
	trusted_oracle_strategy_evaluator.cc)
#	r_singleton.cc \
#	stats_distribution_binned.cc \

LOCAL_C_INCLUDES := $(INSTRUMENTS_INCLUDES)
LOCAL_CXXFLAGS := $(common_CXXFLAGS)
ifneq ($(my_PROFILING_BUILD),)
LOCAL_CXXFLAGS += $(my_PROFILING_CFLAGS)
LOCAL_STATIC_LIBRARIES += andprof
LOCAL_LDLIBS += -llog
endif
LOCAL_LDFLAGS += -fuse-ld=gold
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := run_perf_test
LOCAL_SRC_FILES := $(addprefix ../tests/acceptance/, \
	brute_force_performance_test.c)
LOCAL_C_INCLUDES := $(INSTRUMENTS_INCLUDES) $(LOCAL_PATH)/../tests/include
LOCAL_CFLAGS := $(common_CXXFLAGS)
ifneq ($(my_PROFILING_BUILD),)
LOCAL_CFLAGS += $(my_PROFILING_CFLAGS)
LOCAL_STATIC_LIBRARIES += andprof
LOCAL_LDLIBS += -llog
endif
LOCAL_LDFLAGS += -fuse-ld=gold
LOCAL_SHARED_LIBRARIES := libinstruments
include $(BUILD_EXECUTABLE)