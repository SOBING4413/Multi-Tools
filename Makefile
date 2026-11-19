# ---------------------------------------------------------------------------
#  MultiTool - MinGW-w64 build
#    make                 build multi-tool.exe (x64 Release)
#    make DEBUG=1         build with debug symbols, no optimisation
#    make clean           remove build output
#    make i18n            regenerate src/i18n/* from tools/language_source.py
#  Requires: x86_64-w64-mingw32-gcc + windres  (MSYS2: pacman -S mingw-w64-x86_64-gcc make)
#  On MSYS2 itself use:  make CC=gcc WINDRES=windres
# ---------------------------------------------------------------------------
CC      = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres
PYTHON  = python3

TARGET  = multi-tool.exe
BUILD   = build

ifeq ($(DEBUG),1)
  OPT = -O0 -g
  LDOPT =
else
  OPT = -O2
  LDOPT = -s
endif

CFLAGS  = -std=gnu11 $(OPT) -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -Wno-missing-field-initializers \
          -MMD -MP -Isrc -Ires -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00
LDFLAGS = -mwindows -static-libgcc $(LDOPT)
LIBS    = -lgdi32 -luser32 -lkernel32 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -lcomctl32 -luxtheme -ldwmapi \
          -lws2_32 -liphlpapi -lwinhttp -lnetapi32 -lsetupapi -lcfgmgr32 -lbcrypt -lwintrust -lcrypt32 -lpsapi -lpowrprof -limm32 -lmsimg32

# every .c below src/ (up to 3 levels deep)
SRC     = $(wildcard src/*.c src/*/*.c src/*/*/*.c)
OBJ     = $(SRC:src/%.c=$(BUILD)/%.o)
RES     = $(BUILD)/res/multitool.res.o
DEPS    = $(OBJ:.o=.d)

all: $(TARGET)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(RES): res/multitool.rc res/multitool.manifest res/app.ico res/resource.h src/core/version.h
	@mkdir -p $(dir $@)
	$(WINDRES) -Ires -Isrc --input res/multitool.rc --output $@ --output-format=coff

$(TARGET): $(OBJ) $(RES)
	$(CC) -o $@ $(OBJ) $(RES) $(LDFLAGS) $(LIBS)

i18n:
	$(PYTHON) tools/gen_i18n.py

clean:
	rm -rf $(BUILD) $(TARGET)

-include $(DEPS)

.PHONY: all clean i18n
