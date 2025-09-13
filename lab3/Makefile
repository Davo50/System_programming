# Makefile для Windows (MinGW / MSYS2)
# Сборка двух программ lab3_task1.exe и lab3_task2.exe (WinAPI)
# Использование:
#   make          # собрать оба бинарника
#   make lab3_task1.exe
#   make lab3_task2.exe
#   make clean

# Компилятор можно переопределить через переменную окружения:
#   make CC=x86_64-w64-mingw32-gcc
CC ?= gcc

CFLAGS ?= -std=c11 -O2 -Wall -Wextra -D_WIN32_WINNT=0x0601
LDFLAGS ?=

SRC1 = lab3_task1.c
SRC2 = lab3_task2.c

TARGET1 = lab3_task1.exe
TARGET2 = lab3_task2.exe

TARGETS = $(TARGET1) $(TARGET2)

all: $(TARGETS)

$(TARGET1): $(SRC1)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(TARGET2): $(SRC2)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# удобные цели для сборки с отладкой или санитайзерами (если поддерживает ваша toolchain)
debug: CFLAGS += -g -O0
debug: clean all

san: CFLAGS += -g -O1 -fsanitize=address,undefined
san: LDFLAGS += -fsanitize=address,undefined
san: clean all

clean:
	-@rm -f $(TARGETS) *.o
	-@echo "Clean completed."

.PHONY: all clean debug san
