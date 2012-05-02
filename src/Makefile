LIBRARIES := libinstruments.a
OBJS := instruments.o estimator.o estimator_set.o 

CXXFLAGS := -g -Wall -Werror -I../include

all: $(LIBRARIES)

libinstruments.a: $(OBJS)
	ar rcs $@ $(OBJS)

clean:
	rm -f $(OBJS) $(LIBRARIES) *~

include deps.mk
