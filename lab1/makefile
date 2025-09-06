CC := gcc
CFLAGS := -std=c99 -O2 -Wall -Wextra -pedantic
DBGFLAGS := -std=c99 -g -O0 -Wall -Wextra -pedantic

SRCS := $(wildcard task*.c)
BINS := $(SRCS:.c=)

.PHONY: all help clean rebuild debug run-all run1 run2 run3 run4 run5

all: $(BINS)

%: %.c
	@echo "Compiling $< -> $@"
	$(CC) $(CFLAGS) $< -o $@

debug: $(SRCS)
	@echo "Compiling debug build for all tasks"
	@for src in $(SRCS); do \
	  bin=$${src%.c}; \
	  $(CC) $(DBGFLAGS) $$src -o $$bin; \
	done

run1: task1
	@./task1

run2: task2
	@./task2

run3: task3
	@./task3

run4: task4
	@./task4

run5: task5
	@./task5

run-all: all
	@echo "Запуск task1..task5 по очереди (прервать можно Ctrl+C)"
	@for b in $(BINS); do \
	  echo "==== Запуск $$b ===="; \
	  ./$$b || exit $$?; \
	  echo; \
	done


rebuild: clean all

clean:
	@echo "Удаляю бинарники: $(BINS)"
	@rm -f $(BINS)

help:
	@printf "Доступные цели:\n"
	@printf "  make         (или make all)   - собрать все taskN\n"
	@printf "  make run1                     - запустить task1\n"
	@printf "  make run2                     - запустить task2\n"
	@printf "  make run3                     - запустить task3\n"
	@printf "  make run4                     - запустить task4\n"
	@printf "  make run5                     - запустить task5\n"
	@printf "  make run-all                  - собрать и запустить все задания последовательно\n"
	@printf "  make debug                    - собрать все с отладочной информацией (-g)\n"
	@printf "  make rebuild                  - clean + all\n"
	@printf "  make clean                    - удалить бинарники\n"
