CC = cl
CFLAGS = /nologo /W3 /Ox

all: producer_consumer.exe readers_writers.exe wait_threads.exe

producer_consumer.exe: producer_consumer.c
	$(CC) $(CFLAGS) producer_consumer.c /Fe:producer_consumer.exe

readers_writers.exe: readers_writers.c
	$(CC) $(CFLAGS) readers_writers.c /Fe:readers_writers.exe

wait_threads.exe: wait_threads.c
	$(CC) $(CFLAGS) wait_threads.c /Fe:wait_threads.exe

clean:
	if exist producer_consumer.exe del producer_consumer.exe
	if exist readers_writers.exe del readers_writers.exe
	if exist wait_threads.exe del wait_threads.exe
	if exist *.obj del *.obj
