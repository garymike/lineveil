# MinGW-w64 build (native on Windows via MSYS2, or cross-compile from Linux/macOS).
#   make                 -> build/lineveil-x64.exe
#   make ARCH=x86        -> build/lineveil-x86.exe
#   make ARCH=arm64      -> build/lineveil-arm64.exe   (needs llvm-mingw)
#   make all-arch        -> all three
#   make test            -> native unit tests (Linux/macOS)
#   make fuzz            -> fuzz the hotkey parser (clang)
# Override tools as usual, e.g. make CC=gcc CXX=g++ WINDRES=windres in MSYS2.

ARCH ?= x64

ifeq ($(ARCH),x64)
  TRIPLE  := x86_64-w64-mingw32
  T_CC    := $(TRIPLE)-gcc
  T_CXX   := $(TRIPLE)-g++
  ENTRY   := entry
  ARCH_LD := -Wl,--high-entropy-va
  ARCH_CF := -fno-asynchronous-unwind-tables
  RTLIB   := -lgcc
  SUBSYS  := 6.1
else ifeq ($(ARCH),x86)
  TRIPLE  := i686-w64-mingw32
  T_CC    := $(TRIPLE)-gcc
  T_CXX   := $(TRIPLE)-g++
  ENTRY   := _entry
  ARCH_LD := -Wl,--large-address-aware
  ARCH_CF := -fno-asynchronous-unwind-tables
  RTLIB   := -lgcc
  SUBSYS  := 6.1
else ifeq ($(ARCH),arm64)
  TRIPLE  := aarch64-w64-mingw32
  T_CC    := $(TRIPLE)-clang
  T_CXX   := $(TRIPLE)-clang++
  ENTRY   := entry
  ARCH_LD := -Wl,--high-entropy-va
  ARCH_CF :=
  RTLIB   :=
  SUBSYS  := 6.2
else
  $(error ARCH must be x64, x86, or arm64)
endif

ifeq ($(origin CC),default)
CC := $(T_CC)
endif
ifeq ($(origin CXX),default)
CXX := $(T_CXX)
endif
WINDRES ?= $(TRIPLE)-windres
STRIP   ?= $(TRIPLE)-strip

OUT  := build/lineveil-$(ARCH).exe
OBJ  := build/obj-$(ARCH)
CSRC := $(wildcard src/*.c)
XSRC := $(wildcard src/*.cpp)
OBJS := $(CSRC:src/%.c=$(OBJ)/%.o) $(XSRC:src/%.cpp=$(OBJ)/%.o) $(OBJ)/lineveil.res.o
HDR  := $(wildcard src/*.h) res/version.h

DEFS := -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00
COMMON := -Os -Wall -Wextra -fno-builtin -fno-stack-protector \
          -fno-ident -ffunction-sections -fdata-sections $(ARCH_CF)

# C++ skips -ffreestanding: the DirectX headers pull in intrinsics headers that
# expect a hosted environment. Nothing from the C++ runtime is linked either way.
CFLAGS   := -std=c99 -ffreestanding $(DEFS) $(COMMON)
CXXFLAGS := -std=c++11 $(DEFS) $(COMMON) -fno-exceptions -fno-rtti -fno-threadsafe-statics

LDFLAGS := -nostdlib -nostartfiles -e $(ENTRY) \
           -Wl,--subsystem,windows:$(SUBSYS) -Wl,--gc-sections \
           -Wl,--dynamicbase -Wl,--nxcompat $(ARCH_LD) -s

# Only the four classic system DLLs are import-linked. d3d11/dcomp are
# loaded at runtime by the DComp renderer, and only when it's selected.
LIBS := -lkernel32 -luser32 -lgdi32 -lshell32 $(RTLIB)

$(OUT): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)
	$(STRIP) --strip-all $@
	@ls -l $@

$(OBJ)/%.o: src/%.c $(HDR) | $(OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/%.o: src/%.cpp $(HDR) | $(OBJ)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)/lineveil.res.o: res/lineveil.rc res/version.h res/lineveil.ico res/lineveil.manifest | $(OBJ)
	$(WINDRES) -I res -O coff res/lineveil.rc -o $@

$(OBJ):
	@mkdir -p $@

# Native unit tests for pure-logic code (runs on Linux/macOS, no Windows needed).
HOSTCC ?= cc
TEST_FLAGS := -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -g \
              -fsanitize=address,undefined -fno-sanitize-recover=all -Itests/shim

test:
	@mkdir -p build
	$(HOSTCC) $(TEST_FLAGS) tests/test_hotkey.c src/hotkey.c -o build/test_hotkey
	$(HOSTCC) $(TEST_FLAGS) tests/test_pattern.c src/pattern.c -o build/test_pattern
	./build/test_hotkey
	./build/test_pattern

# Coverage-guided fuzzing of the hotkey parser (needs clang). FUZZ_TIME in seconds.
FUZZ_TIME ?= 60
fuzz:
	@mkdir -p build/corpus
	clang -std=c99 -D_POSIX_C_SOURCE=200809L -g -O1 -Itests/shim \
	  -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all \
	  tests/fuzz_hotkey.c src/hotkey.c -o build/fuzz_hotkey
	./build/fuzz_hotkey -max_total_time=$(FUZZ_TIME) -max_len=190 build/corpus

# Regenerate res/lineveil.ico (needs Python 3 + Pillow). The .ico is committed.
icon:
	python3 tools/make_icon.py

all-arch:
	$(MAKE) ARCH=x64
	$(MAKE) ARCH=x86
	$(MAKE) ARCH=arm64

clean:
	rm -rf build

.PHONY: all-arch clean test fuzz icon
