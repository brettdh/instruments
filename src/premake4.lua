dofile("../common/r_premake.lua")

project "InstrumentsLibrary"
  kind "SharedLib"
  language "C++"
  files { "**.cc" }

  includedirs { ".", "./evaluators", "./joint_distributions", "../../libcmm" }

  flags { "Symbols", "FatalWarnings" }
  links { "pthread", "mocktime", "flipflop" }

  buildoptions { R_buildoptions(), "-Wall", "-std=c++11" }
  linkoptions { R_linkoptions() }
  
  configuration "Debug"
    targetname "instruments_debug"

  configuration "Release"
    targetname "instruments"
    flags { "Optimize" }
    defines { "NDEBUG" }
