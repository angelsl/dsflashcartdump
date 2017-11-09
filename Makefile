.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif
include $(DEVKITARM)/ds_rules

TARGET		:=	dsflashcartdump
BUILD		:=	obj

CXXFILES	:=	main.cpp \
				$(foreach dir,$(TOPDIR)/flashcart_core $(TOPDIR)/flashcart_core/devices,$(notdir $(wildcard $(dir)/*.cpp)))

ARCH		:=	-mthumb -mthumb-interwork
CXXFLAGS	:=	-g $(ARCH) -O2 -fdiagnostics-color=always \
				-D_GNU_SOURCE -DARM9 -DNCGC_PLATFORM_NTR \
				-Wall -Wextra -pedantic -std=c++14 \
				-march=armv5te -mtune=arm946e-s \
				-fomit-frame-pointer -ffast-math \
				-fno-rtti -fno-exceptions -fno-use-cxa-atexit \
				-isystem $(LIBNDS)/include -isystem $(LIBFAT)/include \
				-I$(TOPDIR)/fix_flashcart_core \
				-I$(TOPDIR)/flashcart_core \
				-I$(TOPDIR)/libncgc/include
ASFLAGS		:=	-g $(ARCH)
LDFLAGS		=	-specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS		:=	-lncgc -lfat -lnds9

ifneq ($(BUILD),$(notdir $(CURDIR)))

export TOPDIR	:=	$(CURDIR)

.PHONY: $(BUILD) libncgc clean

$(BUILD): libncgc
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

libncgc:
	@$(MAKE) PLATFORM=ntr -C $(CURDIR)/libncgc

clean:
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds

else

export LD		:=	$(CXX)
export VPATH	:=	$(TOPDIR)/src $(TOPDIR)/flashcart_core $(TOPDIR)/flashcart_core/devices
export OUTPUT	:=	$(TOPDIR)/$(TARGET)
export DEPSDIR	:=	$(TOPDIR)/$(BUILD)
export OFILES	:=	$(BINFILES:.bin=.o) $(CXXFILES:.cpp=.o)
export LIBPATHS	:=	-L$(LIBNDS)/lib -L$(TOPDIR)/libncgc/out/ntr
DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).nds: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

%.o: %.bin
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

endif
