# Usage:
#   nmake           - build both tools
#   nmake tool      - build integrity_tool.exe
#   nmake service   - build integrity_service.exe
#   nmake clean

CC=cl
CFLAGS=/nologo /W3 /Ox /MD
LIBS=advapi32.lib

all: tool service

tool:
	$(CC) $(CFLAGS) /Fe:integrity_tool.exe integrity_tool.c $(LIBS)

service:
	$(CC) $(CFLAGS) /Fe:integrity_service.exe integrity_service.c $(LIBS)

clean:
	del /Q integrity_tool.exe integrity_service.exe *.obj || echo nothing to remove