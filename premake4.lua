solution "Instruments"
  configurations { "Debug", "Release" }
  
  includedirs { "./include" }
  buildoptions { "-std=gnu++0x" }

  include "src"
  include "tests/unit"
  include "tests/acceptance"
