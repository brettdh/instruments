LIB = libinstruments.a
OBJS = instruments.o

CFLAGS = -g -Wall -Werror -I../include

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $(OBJS)

clean:
	rm -f $(OBJS) $(LIB)
