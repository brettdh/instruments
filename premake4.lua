solution "Instruments"
  configurations { "Debug", "Release" }
  
  includedirs { "./include", "../nested_array" }

  include "src"
  include "tests/unit"
  include "tests/acceptance"

  newaction {
    trigger = "install",
    description = "Install to /usr/local",
    execute = function ()
      os.execute("cp include/instruments.h /usr/local/include/")
      os.execute("cp include/estimator_bound.h /usr/local/include/")
      os.execute("cp src/eval_method.h /usr/local/include/")
      os.execute("cp src/libinstruments.so /usr/local/lib/")
    end
  }
