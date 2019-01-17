TARGET	= kbd_bootldr
SRC		= main.c usb_desc.c $(LUFA_SRC_USB) $(LUFA_SRC_USBCLASS)

MCU		= atmega16u2
ARCH	= AVR8
F_CPU	= 16000000
F_USB	= $(F_CPU)

AVRDUDE_LOCK		= 0x3F
AVRDUDE_LFUSE		= 0xFE
AVRDUDE_HFUSE		= 0xD8
AVRDUDE_EFUSE		= 0xFF
AVRDUDE_PROGRAMMER	= usbasp

LTO				= Y
OPTIMIZATION	= s

C_FLAGS			+= -fdata-sections -Wno-misleading-indentation -DUSE_LUFA_CONFIG_HEADER
LD_FLAGS		+= -Wl,--section-start=".text=0x3000"

LUFA_PATH		= lufa/LUFA
DMBS_PATH		= $(LUFA_PATH)/Build/DMBS/DMBS
DMBS_LUFA_PATH	= $(LUFA_PATH)/Build/LUFA

include $(DMBS_LUFA_PATH)/lufa-sources.mk
include $(DMBS_LUFA_PATH)/lufa-gcc.mk

include $(DMBS_PATH)/core.mk
include $(DMBS_PATH)/cppcheck.mk
include $(DMBS_PATH)/doxygen.mk
include $(DMBS_PATH)/dfu.mk
include $(DMBS_PATH)/gcc.mk
include $(DMBS_PATH)/hid.mk
include $(DMBS_PATH)/avrdude.mk

all:
