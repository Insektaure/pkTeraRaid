#---------------------------------------------------------------------------------
# Tesla/Ultrahand overlay for pkTeraRaid - PLA quick-glance shiny scanner.
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
APP_TITLE   := pkTeraRaid
APP_AUTHOR  := Insektaure
APP_VERSION := 1.0.0
TARGET      := pkTeraRaid
BUILD       := build

# Overlay sources (own) + shared PLA core from the parent project.
SOURCES     := source ../source/pla
INCLUDES    := include ../include

NO_ICON     := 1

# Pull in libultrahand sources + includes.
include $(TOPDIR)/lib/libultrahand/ultrahand.mk

#---------------------------------------------------------------------------------
ARCH := -march=armv8-a+simd+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS := -g -Wall -O2 -ffunction-sections -fdata-sections -flto \
          -fuse-linker-plugin -fomit-frame-pointer -fno-strict-aliasing \
          $(ARCH) $(DEFINES)

CFLAGS += $(INCLUDE) -D__SWITCH__ -DAPP_VERSION=\"$(APP_VERSION)\" -D_FORTIFY_SOURCE=2

# libultrahand options
UI_OVERRIDE_PATH := /config/pkTeraRaid/
CFLAGS += -DUI_OVERRIDE_PATH=\"$(UI_OVERRIDE_PATH)\"
CFLAGS += -DUSING_WIDGET_DIRECTIVE=1
CFLAGS += -DUSE_EXCEPTION_WRAP=1

LDFLAGS += -Wl,-wrap,__cxa_throw \
           -Wl,-wrap,_Unwind_Resume \
           -Wl,-wrap,__gxx_personality_v0

CXXFLAGS := $(CFLAGS) -std=c++26 -Wno-dangling-else -ffast-math \
            -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables \
            -ffunction-sections -fdata-sections

ASFLAGS := $(ARCH)
LDFLAGS += -specs=$(DEVKITPRO)/libnx/switch.specs $(ARCH) -Wl,-Map,$(notdir $*.map) \
           -Wl,--as-needed -Wl,--gc-sections

LIBS := -lcurl -lz -lminizip -lmbedtls -lmbedx509 -lmbedcrypto -ldmntcht -lnx

LIBDIRS := $(CURDIR)/.. $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)

export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
    export LD := $(CC)
else
    export LD := $(CXX)
endif

export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_SRC)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -fr $(BUILD) $(TARGET).ovl $(TARGET).nro $(TARGET).elf

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).ovl

$(OUTPUT).ovl: $(OUTPUT).nro
	@mv $< $@
	@echo "built ... $(notdir $@)"

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
	@elf2nro $< $@ --nacp=$(OUTPUT).nacp
	@echo "nro    ... $(notdir $@)"

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
#---------------------------------------------------------------------------------
