project "AcceptanceTests"
  kind "ConsoleApp"
  language "C"
  files { "*.c" }
  excludes { "performance_test.c" }
  
  includedirs { "../include", "../../include", "../../src" }
  libdirs { "../../src" }
  links { "InstrumentsLibrary" }
  linkoptions { "-Wl,-rpath,../../src" }
  targetname "run_tests"

  configuration "Debug"
    targetsuffix "_debug"
    flags { "Symbols" }

project "PerformanceTests"
  kind "ConsoleApp"
  language "C"
  files { "performance_test.c" }

  includedirs { "../include", "../../include", "../../src" }
  libdirs { "../../src" }
  links { "InstrumentsLibrary" }
  linkoptions { "-Wl,-rpath,../../src" }
  targetname "run_perf_test"

  configuration "Debug"
    targetsuffix "_debug"
    flags { "Symbols" }
