# Generate header dependency rules
#   see http://stackoverflow.com/questions/204823/
# ---
DEP_SRCS := $(wildcard ./*.cc) $(wildcard */*.cc)
DEP_DIRS := $(dir $(DEP_SRCS))
DEP_NOTDIRS:= $(patsubst %.cc, .%.dep, $(notdir $(DEP_SRCS)))
DEPS := $(join $(DEP_DIRS), $(DEP_NOTDIRS))

.%.dep: %.cc
	$(CXX) -MM $(CXXFLAGS) $< >$@

include $(DEPS)
