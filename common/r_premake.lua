function capture(cmd, input)
   input = input or ""
   local tmpfilename = os.tmpname()
   local save_cmd = cmd .. " >" .. tmpfilename
   local pipe = assert(io.popen(save_cmd, "w"))
   pipe:write(input)
   pipe:close()
   local output = assert(io.input(tmpfilename))
   local string = output:read("*all")
   output:close()
   os.remove(tmpfilename)

   -- trim whitespace (http://lua-users.org/wiki/CommonFunctions)
   return string:gsub("^%s*(.-)%s*$", "%1")
end

function R_buildoptions()
   local R_HOME = capture("R RHOME")

   local RCPPFLAGS = capture(R_HOME .. "/bin/R CMD config --cppflags")
   local RCPPINCL =	capture(R_HOME .. "/bin/R --vanilla --slave", "Rcpp:::CxxFlags()")
   local RINSIDEINCL = capture(R_HOME .. "/bin/R --vanilla --slave", "RInside:::CxxFlags()")
   local R_BUILD_ALL = table.concat({ RCPPFLAGS, RCPPINCL, RINSIDEINCL }, " ")
   return R_BUILD_ALL
end

function R_linkoptions()
   local R_HOME = capture("R RHOME")

   local RLDFLAGS = capture(R_HOME .. "/bin/R CMD config --ldflags")
   local RBLAS = capture(R_HOME .. "/bin/R CMD config BLAS_LIBS")
   local RLAPACK = capture(R_HOME .. "/bin/R CMD config LAPACK_LIBS")
   local RCPPLIBS = capture(R_HOME .. "/bin/R --vanilla --slave", "Rcpp:::LdFlags()")
   local RINSIDELIBS = capture(R_HOME .. "/bin/R --vanilla --slave", "RInside:::LdFlags()")

   local R_LINK_ALL = table.concat({ RLDFLAGS, RBLAS, RLAPACK, RCPPLIBS, RINSIDELIBS }, " ")
   return R_LINK_ALL
end
