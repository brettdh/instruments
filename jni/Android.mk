LOCAL_PATH := $(call my-dir)

my_PROFILING_BUILD := yes
my_PROFILING_CFLAGS := -pg -DPROFILING_BUILD

common_CFLAGS:=-g -Wall -Werror -DANDROID -O3
common_CXXFLAGS:=$(common_CFLAGS) -std=gnu++0x
INSTRUMENTS_INCLUDES := \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../src/evaluators \
	$(LOCAL_PATH)/../src/joint_distributions

ANDROID_LIBS_ROOT := $(LOCAL_PATH)/../../android-source/external/bdh_apps

MOCKTIME_ROOT := $(ANDROID_LIBS_ROOT)/mocktime
MOCKTIME_INCLUDES := $(MOCKTIME_ROOT)
LIBPT_ROOT := $(ANDROID_LIBS_ROOT)/libpowertutor/cpp_source
LIBPT_INCLUDES := $(LIBPT_ROOT)
INTNW_ROOT := $(ANDROID_LIBS_ROOT)/libcmm

include $(CLEAR_VARS)

LOCAL_MODULE := powertutor
LOCAL_SRC_FILES := ../$(LIBPT_ROOT)/obj/local/$(TARGET_ARCH_ABI)/libpowertutor.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := mocktime
LOCAL_SRC_FILES := ../$(MOCKTIME_ROOT)/obj/local/$(TARGET_ARCH_ABI)/libmocktime.so
include $(PREBUILT_SHARED_LIBRARY)


#include $(CLEAR_VARS)

#LOCAL_MODULE := flipflop
#LOCAL_SRC_FILES := ../$(INTNW_ROOT)/obj/local/$(TARGET_ARCH_ABI)/libflipflop.a
#include $(PREBUILT_STATIC_LIBRARY)



include $(CLEAR_VARS)
LOCAL_CPP_EXTENSION:=.cc
LOCAL_MODULE := instruments
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/src
LOCAL_SRC_FILES := $(addprefix ../src/, \
	abstract_joint_distribution.cc \
	continuous_distribution.cc \
	debug.cc \
	error_calculation.cc \
	error_weight_params.cc \
	estimator.cc \
	estimator_registry.cc \
	eval_method.cc \
	evaluators/bayesian_strategy_evaluator.cc \
	evaluators/confidence_bounds_strategy_evaluator.cc \
	evaluators/empirical_error_strategy_evaluator.cc \
	evaluators/trusted_oracle_strategy_evaluator.cc \
	evaluators/students_t.cc \
	external_estimator.cc \
	goal_adaptive_resource_weight.cc \
	instruments.cc \
	generic_joint_distribution.cc \
	joint_distributions/intnw_joint_distribution.cc \
	joint_distributions/remote_exec_joint_distribution.cc \
	resource_weights.cc \
	running_mean_estimator.cc \
	stats_distribution.cc \
	stats_distribution_all_samples.cc \
	stats_distribution_binned.cc \
	stopwatch.cc \
	strategy.cc \
	strategy_evaluator.cc \
	thread_pool.cc \
	timeops.cc)
#	r_singleton.cc \

LOCAL_C_INCLUDES := $(INSTRUMENTS_INCLUDES) $(MOCKTIME_INCLUDES) $(LIBPT_INCLUDES) $(INTNW_ROOT)
LOCAL_CXXFLAGS := $(common_CXXFLAGS)
#LOCAL_STATIC_LIBRARIES += flipflop
ifneq ($(my_PROFILING_BUILD),)
LOCAL_CXXFLAGS += $(my_PROFILING_CFLAGS)
LOCAL_STATIC_LIBRARIES += android-ndk-profiler 
LOCAL_LDLIBS += -llog
endif
LOCAL_SHARED_LIBRARIES := mocktime powertutor
#LOCAL_LDFLAGS += -fuse-ld=gold
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := run_perf_test
LOCAL_SRC_FILES := $(addprefix ../tests/acceptance/, \
	performance_test.c)
LOCAL_C_INCLUDES := $(INSTRUMENTS_INCLUDES) $(LOCAL_PATH)/../tests/include
LOCAL_CFLAGS := $(common_CFLAGS)
ifneq ($(my_PROFILING_BUILD),)
LOCAL_CFLAGS += $(my_PROFILING_CFLAGS)
LOCAL_STATIC_LIBRARIES += android-ndk-profiler
LOCAL_LDLIBS += -llog
endif
#LOCAL_LDFLAGS += -fuse-ld=gold
LOCAL_SHARED_LIBRARIES := libinstruments libpowertutor libmocktime
include $(BUILD_EXECUTABLE)


ifneq ($(my_PROFILING_BUILD),)
$(call import-module,android-ndk-profiler)
endif
