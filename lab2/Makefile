# Makefile for Lab2 variant5 tasks
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic
TARGETS = task1 task2 task3 task4 task5 task6 task7

all: $(TARGETS)

task1: task1.c
	$(CC) $(CFLAGS) -o $@ $<

task2: task2.c
	$(CC) $(CFLAGS) -o $@ $<

task3: task3.c
	$(CC) $(CFLAGS) -o $@ $<

task4: task4.c
	$(CC) $(CFLAGS) -o $@ $< -lm

task5: task5.c
	$(CC) $(CFLAGS) -o $@ $< -lm

task6: task6.c
	$(CC) $(CFLAGS) -o $@ $<

task7: task7.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean
