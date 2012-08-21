LOCAL_PATH := $(call my-dir)

common_CXXFLAGS:=-g -O3 -Wall -Werror -DANDROID
INSTRUMENTS_INCLUDES := \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../src

include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION:=.cc
LOCAL_MODULE := instruments
LOCAL_SRC_FILES := $(addprefix ../src/, \
	empirical_error_strategy_evaluator.cc \
	estimator.cc \
	estimator_registry.cc \
	external_estimator.cc \
	goal_adaptive_resource_weight.cc \
	instruments.cc \
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
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := run_perf_test
LOCAL_SRC_FILES := $(addprefix ../tests/acceptance/, \
	brute_force_performance_test.c)
LOCAL_C_INCLUDES := $(INSTRUMENTS_INCLUDES) $(LOCAL_PATH)/../tests/include
LOCAL_CFLAGS := $(common_CXXFLAGS)
LOCAL_SHARED_LIBRARIES := libinstruments
include $(BUILD_EXECUTABLE)
