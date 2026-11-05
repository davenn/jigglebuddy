# Cross-compile from Linux/macOS/MSYS2 with MinGW-w64:
#   make            -> jigglebuddy.exe
ifeq ($(origin CC),default)
CC := x86_64-w64-mingw32-gcc
endif
CFLAGS  := -Os -Wall -Wextra -fno-asynchronous-unwind-tables -fno-ident \
           -fno-stack-protector -fno-builtin
LDFLAGS := -s -nostdlib -mwindows -Wl,--entry=entry -Wl,--gc-sections
LIBS    := -luser32 -lkernel32 -lshell32 -lgdi32

jigglebuddy.exe: jigglebuddy.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

clean:
	rm -f jigglebuddy.exe

.PHONY: clean
