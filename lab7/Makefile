CC = cl
CFLAGS = /nologo /W4 /MD /EHsc
LDFLAGS = /link setupapi.lib cfgmgr32.lib advapi32.lib user32.lib
TARGET = UsbMonitorService.exe
SRC = usb_monitor_service.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) /out:$(TARGET)

clean:
	-if exist $(TARGET) del $(TARGET)
