#---------------------------------------------------------------------------------
# Mario Kart DS NX - build with devkitPro (devkitA64 + libnx + switch portlibs)
#
#   make            patches + builds the melonDS core (first time only), then mkds-nx.nro
#   make clean      removes the frontend build (keeps the core)
#   make distclean  also cleans the core
#
# melonDS is a git submodule (third_party/melonDS); clone with --recursive.
# Without a local devkitPro install, build in Docker:
#   docker run --rm -v "$PWD":/src -w /src devkitpro/devkita64 make
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
TARGET      := mkds-nx
BUILD       := build
SOURCES     := source
DATA        :=
INCLUDES    := include
ROMFS       := romfs

APP_TITLE   := Mario Kart DS NX
APP_AUTHOR  := homebrew
APP_VERSION := 1.0.0
ICON        := icon.jpg

MELONDS     := $(TOPDIR)/third_party/melonDS
CORE_LIB    := $(MELONDS)/melonds_libretro_libnx.a

# A statically linked libretro core expects the frontend to provide these
# libretro-common helpers (file streams, paths, strlcpy).
LRC         := $(MELONDS)/src/libretro/libretro-common
LRC_DIRS    := compat encodings file streams string vfs
LRC_FILES   := compat_strl.c fopen_utf8.c compat_posix_string.c compat_strcasestr.c \
               encoding_utf.c file_path.c file_stream.c file_stream_transforms.c \
               stdstring.c vfs_implementation.c

#---------------------------------------------------------------------------------
ARCH     := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS   := -g -Wall -O2 -ffunction-sections $(ARCH) $(DEFINES)
CFLAGS   += $(INCLUDE) -D__SWITCH__ -DSWITCH=1 -DHAVE_LIBNX=1 `$(PORTLIBS)/bin/$(PREFIX)pkg-config --cflags sdl2 SDL2_ttf`
CFLAGS   += -I$(MELONDS)/src/libretro/libretro-common/include

CXXFLAGS := $(CFLAGS) -std=gnu++17 -fno-rtti

ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

# The core goes first so its references to SDL/GL/libnx resolve after it.
LIBS     := $(CORE_LIB) `$(PORTLIBS)/bin/$(PREFIX)pkg-config --libs SDL2_ttf sdl2` -lnx

LIBDIRS  := $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LRC_DIRS),$(LRC)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c))) $(LRC_FILES)
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export LD := $(CXX)
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_SRC)
export INCLUDE    := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                     $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                     -I$(CURDIR)/$(BUILD)
export LIBPATHS   := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export APP_ICON     := $(TOPDIR)/$(ICON)
export NROFLAGS     += --icon=$(APP_ICON) --nacp=$(CURDIR)/$(TARGET).nacp --romfsdir=$(CURDIR)/$(ROMFS)

.PHONY: $(BUILD) clean distclean all core

all: core $(BUILD)

# melonDS libretro core, with the ARM64 JIT compiled in (CPU MODE: FAST).
# No OpenGL: the frontend scales the software-rendered screens itself.
core: $(CORE_LIB)
$(CORE_LIB):
	@[ -f $(MELONDS)/Makefile ] || { echo "third_party/melonDS is empty: run git submodule update --init"; exit 1; }
	@echo applying patches/melonDS-switch.patch ...
	@cd $(MELONDS) && (git apply --reverse --check $(TOPDIR)/patches/melonDS-switch.patch 2>/dev/null \
		|| git apply $(TOPDIR)/patches/melonDS-switch.patch)
	@echo building melonDS core ...
	@$(MAKE) --no-print-directory -C $(MELONDS) platform=libnx JIT_ARCH=aarch64 HAVE_OPENGL=0

$(BUILD): $(CORE_LIB)
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf

distclean: clean
	@$(MAKE) --no-print-directory -C $(MELONDS) platform=libnx clean

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).nro

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES) $(CORE_LIB)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
