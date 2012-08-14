# From the RInside source archive:
#   inst/examples/standard/Makefile

## comment this out if you need a different version of R, 
## and set set R_HOME accordingly as an environment variable
## compiler etc settings used in default make rules

ifneq ($@,clean)
R_HOME = 		$(shell R RHOME)

## include headers and libraries for R 
RCPPFLAGS = 		$(shell $(R_HOME)/bin/R CMD config --cppflags)
RLDFLAGS = 		$(shell $(R_HOME)/bin/R CMD config --ldflags)
RBLAS = 		$(shell $(R_HOME)/bin/R CMD config BLAS_LIBS)
RLAPACK = 		$(shell $(R_HOME)/bin/R CMD config LAPACK_LIBS)

## include headers and libraries for Rcpp interface classes
RCPPINCL = 		$(shell echo 'Rcpp:::CxxFlags()' | $(R_HOME)/bin/R --vanilla --slave)
RCPPLIBS = 		$(shell echo 'Rcpp:::LdFlags()'  | $(R_HOME)/bin/R --vanilla --slave)


## include headers and libraries for RInside embedding classes
RINSIDEINCL = 		$(shell echo 'RInside:::CxxFlags()' | $(R_HOME)/bin/R --vanilla --slave)
RINSIDELIBS = 		$(shell echo 'RInside:::LdFlags()'  | $(R_HOME)/bin/R --vanilla --slave)
endif

# (mine)

all: R_generated_vars.mk

R_generated_vars.mk: Makefile
	@rm -f $@
	@echo "R_CPPFLAGS := 		-Wall $(shell $(R_HOME)/bin/R CMD config CPPFLAGS)" >>$@
	@echo "R_CXXFLAGS := 		$(RCPPFLAGS) $(RCPPINCL) $(RINSIDEINCL) $(shell $(R_HOME)/bin/R CMD config CXXFLAGS)" >>$@
	@echo "R_LDLIBS := 		$(RLDFLAGS) $(RRPATH) $(RBLAS) $(RLAPACK) $(RCPPLIBS) $(RINSIDELIBS)" >>$@

clean:
	rm -f R_generated_vars.mk
