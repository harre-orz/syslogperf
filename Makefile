TARGET = syslogperf
CC = gcc
CFLAGS = -W -Wall -O2
SRCS = syslogperf.c
OBJS = $(SRCS:.c=.o)


.PHONY: all clean
.SUFFIXES: .c .o
.DEFAULT: all

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -rf *.o *.d *.so *.a

.c.o:
	$(CC) $(CFLAGS) -c $<
