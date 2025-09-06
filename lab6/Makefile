CC=cl
CFLAGS=/nologo /W3 /MD /DWIN32 /D_WINDOWS /O2
LINKFLAGS=/nologo

all: socket_server.exe socket_client.exe pipe_server.exe pipe_client.exe

socket_server.exe: socket_server.c
	$(CC) $(CFLAGS) socket_server.c ws2_32.lib /link $(LINKFLAGS) /OUT:socket_server.exe

socket_client.exe: socket_client.c
	$(CC) $(CFLAGS) socket_client.c ws2_32.lib /link $(LINKFLAGS) /OUT:socket_client.exe

pipe_server.exe: pipe_server.c
	$(CC) $(CFLAGS) pipe_server.c /link $(LINKFLAGS) /OUT:pipe_server.exe

pipe_client.exe: pipe_client.c
	$(CC) $(CFLAGS) pipe_client.c /link $(LINKFLAGS) /OUT:pipe_client.exe

clean:
	del /Q socket_server.exe socket_client.exe pipe_server.exe pipe_client.exe *.obj 2>nul || echo
