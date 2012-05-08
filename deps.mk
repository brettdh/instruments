# Generate header dependency rules
#   see http://stackoverflow.com/questions/204823/
# ---
DEP_SRCS=$(wildcard *.cc)
DEPS=$(DEP_SRCS:%.cc=.%.dep)

.%.dep: %.cc
	$(CXX) -MM $(CXXFLAGS) $< >$@

include $(DEPS)
