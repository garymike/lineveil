# MinGW-w64 build (native on Windows via MSYS2, or cross-compile from Linux/macOS).
#   make                 -> build/scanlite-x64.exe
#   make ARCH=x86        -> build/scanlite-x86.exe
#   make ARCH=arm64      -> build/scanlite-arm64.exe   (needs llvm-mingw)
#   make all-arch        -> all three
#   make CC=gcc          -> override compiler, e.g. native MSYS2

ARCH ?= x64

ifeq ($(ARCH),x64)
  TRIPLE   := x86_64-w64-mingw32
  TOOL_CC  := $(TRIPLE)-gcc
  ENTRY    := entry
  ARCH_LD  := -Wl,--high-entropy-va
  ARCH_CF  := -fno-asynchronous-unwind-tables
  RTLIB    := -lgcc
else ifeq ($(ARCH),x86)
  TRIPLE   := i686-w64-mingw32
  TOOL_CC  := $(TRIPLE)-gcc
  ENTRY    := _entry
  ARCH_LD  := -Wl,--large-address-aware
  ARCH_CF  := -fno-asynchronous-unwind-tables
  RTLIB    := -lgcc
else ifeq ($(ARCH),arm64)
  TRIPLE   := aarch64-w64-mingw32
  TOOL_CC  := $(TRIPLE)-clang
  ENTRY    := entry
  ARCH_LD  := -Wl,--high-entropy-va
  ARCH_CF  :=
  RTLIB    :=
else
  $(error ARCH must be x64, x86, or arm64)
endif

ifeq ($(origin CC),default)
CC := $(TOOL_CC)
endif
STRIP ?= $(TRIPLE)-strip

OUT := build/scanlite-$(ARCH).exe
SRC := $(wildcard src/*.c)
HDR := $(wildcard src/*.h)

CFLAGS  := -Os -std=c99 -Wall -Wextra \
           -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN \
           -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 \
           -ffreestanding -fno-builtin -fno-stack-protector -fno-ident \
           -ffunction-sections -fdata-sections $(ARCH_CF)

LDFLAGS := -nostdlib -nostartfiles -e $(ENTRY) \
           -Wl,--subsystem,windows:6.1 -Wl,--gc-sections \
           -Wl,--dynamicbase -Wl,--nxcompat $(ARCH_LD) -s

LIBS    := -lkernel32 -luser32 -lgdi32 -lshell32 $(RTLIB)

$(OUT): $(SRC) $(HDR)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LIBS)
	$(STRIP) --strip-all $@

all-arch:
	$(MAKE) ARCH=x64
	$(MAKE) ARCH=x86
	$(MAKE) ARCH=arm64

clean:
	rm -rf build

.PHONY: all-arch clean
