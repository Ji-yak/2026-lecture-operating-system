CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -O2
TARGET = hashtable_bench

SRCS = main.c hashtable.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c hashtable.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
