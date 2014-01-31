dofile("../../common/r_premake.lua")

_ = require 'underscore'

project "InstrumentsTests"
  kind "ConsoleApp"
  language "C++"

  files { 
     "run_all_tests.cc",
     
     "empirical_error_strategy_evaluator_test.cc",
     "r_test.cc",
     "stats_distribution_test.cc",
     "goal_adaptive_resource_weight_test.cc",
     "multi_dimension_array_test.cc",
     "running_mean_estimator_test.cc",
     "strategy_estimators_discovery_test.cc",
     "test_common.cc",
     "thread_pool_test.cc",
  }
  local support_files = {
     "abstract_joint_distribution.cc",
     "continuous_distribution.cc",
     "evaluators/bayesian_strategy_evaluator.cc",
     "debug.cc",
     "evaluators/confidence_bounds_strategy_evaluator.cc",
     "evaluators/empirical_error_strategy_evaluator.cc",
     "evaluators/students_t.cc",
     "evaluators/trusted_oracle_strategy_evaluator.cc",
     "error_calculation.cc",
     "error_weight_params.cc",
     "estimator.cc",
     "estimator_registry.cc",
     "eval_method.cc",
     "external_estimator.cc",
     "generic_joint_distribution.cc",
     "goal_adaptive_resource_weight.cc",
     "instruments.cc",
     "joint_distributions/intnw_joint_distribution.cc",
     "joint_distributions/remote_exec_joint_distribution.cc",
     "joint_distributions/optimized_generic_joint_distribution.cc",
     "r_singleton.cc",
     "resource_weights.cc",
     "running_mean_estimator.cc",
     "strategy.cc",
     "strategy_evaluator.cc",
     "stats_distribution.cc",
     "stats_distribution_all_samples.cc",
     "stats_distribution_binned.cc",
     "stopwatch.cc",
     "thread_pool.cc",
     "timeops.cc",
  }
  support_files = _.map(support_files, function(f) return "../../src/"..f; end)
  files(support_files)
              
              
  includedirs { "." }
  includedirs(_.map({"", "evaluators", "joint_distributions"},
                    function(dir) return "../../src/"..dir; end))
  includedirs { "../../../libcmm" }

  flags { "Symbols", "FatalWarnings" }
  links { "pthread", "mocktime", "cppunit", "flipflop" }

  buildoptions { R_buildoptions(), "-Wall", "-std=gnu++0x" }
  linkoptions { R_linkoptions() }
  
  targetname "run_unit_tests"
  
  configuration "Debug"
    targetsuffix "_debug"

  configuration "Release"
    flags { "Optimize" }
    defines { "NDEBUG" }

project "MultiArrayPerfTest"
  kind "ConsoleApp"
  language "C++"
  files { "multi_array_perf_test.cc" }
  includedirs { "../../src" }

  targetname "multi_array_perf_test"

  configuration "Debug"
    targetsuffix "_debug"

  configuration "Release"
    flags { "Optimize" }
    defines { "NDEBUG" }


project "MultiArrayPerfTestDeeper"
  kind "ConsoleApp"
  language "C++"
  files { "multi_array_perf_test.cc" }
  includedirs { "../../src" }

  targetname "multi_array_perf_test_deeper"

  configuration "Debug"
    targetsuffix "_debug"

  configuration "Release"
    flags { "Optimize" }
    defines { "NDEBUG" }

