# Makefile для сборки fileops.exe и mapping_tool.exe с помощью MSVC (cl)
# Использование:
# 1) Откройте "Developer Command Prompt for VS" / "x64 Native Tools Command Prompt" (чтобы cl/nmake были в PATH)
# 2) Выполните: nmake        (или просто make, если у вас nmake под именем make)
# 3) Чистка:     nmake clean
# 4) Полная пересборка: nmake rebuild

# Параметры компиляции
CC = cl
CFLAGS = /nologo /W4 /Ox
# /TC - явно C (не C++), но .c расширение обычно достаточное
CFLAGS_C = $(CFLAGS) /TC

# Список целей (исполняемых файлов)
PROGRAMS = fileops.exe mapping_tool.exe

all: $(PROGRAMS)
	@echo.
	@echo Сборка завершена: $(PROGRAMS)
	@echo Запускать из командной строки: fileops.exe или mapping_tool.exe

# fileops.exe
fileops.exe: fileops.c
	@echo Компилирую $@
	$(CC) $(CFLAGS_C) fileops.c /Fefileops.exe

# mapping_tool.exe
mapping_tool.exe: mapping_tool.c
	@echo Компилирую $@
	$(CC) $(CFLAGS_C) mapping_tool.c /Femapping_tool.exe

# Полная пересборка
rebuild: clean all

# Очистка
clean:
	@echo Удаляю артефакты сборки...
	if exist fileops.exe del /Q fileops.exe
	if exist mapping_tool.exe del /Q mapping_tool.exe
	if exist fileops.obj del /Q fileops.obj
	if exist mapping_tool.obj del /Q mapping_tool.obj
	@echo Очистка завершена.

# Подсказка
help:
	@echo Makefile (MSVC)
	@echo.
	@echo Команды:
	@echo   nmake         - собрать все (fileops.exe, mapping_tool.exe)
	@echo   nmake clean   - удалить сгенерированные файлы
	@echo   nmake rebuild - сделать clean и собрать заново
	@echo   nmake help    - показать эту подсказку
