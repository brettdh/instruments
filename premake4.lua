solution "Instruments"
  configurations { "Debug", "Release" }
  
  includedirs { "./include" }

  include "src"
  include "tests/unit"
  include "tests/acceptance"
