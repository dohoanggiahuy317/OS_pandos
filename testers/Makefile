# Makefile for the debug test program

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I../h
OBJS = debug_test.o pcb.o asl.o
TARGET = debug_test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)