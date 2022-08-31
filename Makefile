include setup.mk

CFLAGS          = -Ofast -Wall -Wno-switch -DEXTLIB=210 -DNDEBUG -I src/
LDFLAGS        := -lm -pthread
SOURCE_C        = $(shell find src/* -type f -name '*.c')
SOURCE_DATA    := $(shell find datafiles/* -type f)
SOURCE_O_LINUX := $(foreach f,$(SOURCE_C:.c=.o),bin/linux/$f) $(foreach f,$(SOURCE_DATA:%=%.data.o),bin/linux/$f)
SOURCE_O_WIN32 := $(foreach f,$(SOURCE_C:.c=.o),bin/win32/$f) $(foreach f,$(SOURCE_DATA:%=%.data.o),bin/win32/$f)

RELEASE_EXECUTABLE_LINUX := app_linux/genhdr
RELEASE_EXECUTABLE_WIN32 := app_win32/genhdr.exe

# Make build directories
$(shell mkdir -p bin/ $(foreach dir, \
	$(dir $(SOURCE_O_WIN32)) \
	$(dir $(SOURCE_O_LINUX)) \
	$(dir $(RELEASE_EXECUTABLE_LINUX)) \
	$(dir $(RELEASE_EXECUTABLE_WIN32)) \
	, $(dir)))

.PHONY: default \
		linux \
		win32 \
		clean

default: linux
all: linux win32
linux: $(RELEASE_EXECUTABLE_LINUX)
win32: $(RELEASE_EXECUTABLE_WIN32)
test: $(RELEASE_EXECUTABLE_LINUX)
	@./$< --i ../oot_decomp_latest/ --o test/ --verbose

clean:
	rm -rf bin
	rm -rf test/include
	rm -f $(RELEASE_EXECUTABLE_LINUX)
	rm -f $(RELEASE_EXECUTABLE_WIN32)

include $(PATH_EXTLIB)ext_lib.mk

# # # # # # # # # # # # # # # # # # # #
# LINUX BUILD                         #
# # # # # # # # # # # # # # # # # # # #

-include $(SOURCE_O_LINUX:.o=.d)

bin/linux/%.data.o: %
	@echo "$(PRNT_RSET)[$(PRNT_GREN)g$(ASSET_FILENAME)$(PRNT_RSET)]"
	$(DataFileCompiler) --cc gcc --i $< --o $@

bin/linux/%.o: %.c
	@echo "$(PRNT_RSET)[$(PRNT_PRPL)$(notdir $@)$(PRNT_RSET)]"
	@gcc -c -o $@ $< $(LDFLAGS) $(CFLAGS)
	$(GD_LINUX)

$(RELEASE_EXECUTABLE_LINUX): $(SOURCE_O_LINUX) $(ExtLib_Linux_O)
	@echo "$(PRNT_RSET)[$(PRNT_PRPL)$(notdir $@)$(PRNT_RSET)] [$(PRNT_PRPL)$(notdir $^)$(PRNT_RSET)]"
	@gcc -o $@ $^ $(LDFLAGS) $(CFLAGS_MAIN)

# # # # # # # # # # # # # # # # # # # #
# WIN32 BUILD                         #
# # # # # # # # # # # # # # # # # # # #

-include $(SOURCE_O_WIN32:.o=.d)

bin/win32/%.data.o: %
	@echo "$(PRNT_RSET)[$(PRNT_GREN)g$(ASSET_FILENAME)$(PRNT_RSET)]"
	$(DataFileCompiler) --cc i686-w64-mingw32.static-gcc --i $< --o $@

bin/win32/%.o: %.c
	@echo "$(PRNT_RSET)[$(PRNT_PRPL)$(notdir $@)$(PRNT_RSET)]"
	@i686-w64-mingw32.static-gcc -c -o $@ $< $(LDFLAGS) $(CFLAGS) -D_WIN32
	$(GD_WIN32)

$(RELEASE_EXECUTABLE_WIN32): $(SOURCE_O_WIN32) $(ExtLib_Win32_O)
	@echo "$(PRNT_RSET)[$(PRNT_PRPL)$(notdir $@)$(PRNT_RSET)] [$(PRNT_PRPL)$(notdir $^)$(PRNT_RSET)]"
	@i686-w64-mingw32.static-gcc -o $@ $^ $(LDFLAGS) $(CFLAGS_MAIN) -D_WIN32
