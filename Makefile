ifeq ($(platform),)
    platform = unix
    ifeq ($(shell uname -s),)
        platform = win
    else ifeq ($(shell uname -s),Darwin)
        platform = osx
    endif
endif

ifeq ($(platform),unix)
    TARGET := clock_core_libretro.so
    CC = gcc
    CFLAGS = -fPIC -shared -O2
    LDFLAGS = -shared
else ifeq ($(platform),osx)
    TARGET := clock_core_libretro.dylib
    CC = clang
    CFLAGS = -fPIC -dynamiclib -O2
    LDFLAGS = -dynamiclib
else ifeq ($(platform),win)
    TARGET := clock_core_libretro.dll
    CC = gcc
    CFLAGS = -O2
    LDFLAGS = -shared -static-libgcc
endif

all: $(TARGET)

$(TARGET): libretro.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)