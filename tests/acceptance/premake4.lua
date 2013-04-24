project "AcceptanceTests"
  kind "ConsoleApp"
  language "C"
  files { "*.c" }
  excludes { "performance_test.c" }
  
  includedirs { "../include", "../../include", "../../src" }
  links { "InstrumentsLibrary" }

  configuration "Debug"
    targetname "run_tests_debug"
  configuration "Release"
    targetname "run_tests"

project "PerformanceTests"
  kind "ConsoleApp"
  language "C"
  files { "performance_test.c" }

  includedirs { "../include", "../../include", "../../src" }
  links { "InstrumentsLibrary" }

  configuration "Debug"
    targetname "run_perf_test_debug"
  configuration "Release"
    targetname "run_perf_test"
