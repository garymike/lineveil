# MinGW-w64 build (native on Windows via MSYS2, or cross-compile from Linux/macOS).
#   make            -> build/scanlite.exe
#   make CC=gcc     -> when building natively in MSYS2

ifeq ($(origin CC),default)
CC      := x86_64-w64-mingw32-gcc
endif
STRIP   ?= x86_64-w64-mingw32-strip
OUT     := build/scanlite.exe
SRC     := $(wildcard src/*.c)

CFLAGS  := -Os -std=c99 -Wall -Wextra \
           -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN \
           -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 \
           -ffreestanding -fno-builtin -fno-stack-protector \
           -fno-asynchronous-unwind-tables -fno-ident \
           -ffunction-sections -fdata-sections

LDFLAGS := -nostdlib -nostartfiles -e entry \
           -Wl,--subsystem,windows -Wl,--gc-sections -Wl,--dynamicbase -Wl,--nxcompat \
           -Wl,--high-entropy-va -s

LIBS    := -lkernel32 -luser32 -lgdi32 -lshell32 -lgcc

$(OUT): $(SRC) $(wildcard src/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LIBS)
	$(STRIP) --strip-all $@

clean:
	rm -rf build

.PHONY: clean
